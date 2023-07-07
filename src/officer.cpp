/*
 * This file is part of OpenOrion2
 * Copyright (C) 2023 Martin Doucha
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <cstring>
#include "screen.h"
#include "guimisc.h"
#include "lang.h"
#include "officer.h"

#define LEADER_LIST_ARCHIVE "officer.lbx"
#define ASSET_LEADERLIST_BG 0
#define ASSET_LEADERLIST_STARSYSTEM_BG 2
#define ASSET_LEADERLIST_ADMINS_PANEL 3
#define ASSET_LEADERLIST_CAPTAINS_PANEL 4
#define ASSET_LEADERLIST_SCROLL_UP_BUTTON 5
#define ASSET_LEADERLIST_SCROLL_DOWN_BUTTON 6
#define ASSET_LEADERLIST_PREV_BUTTON 7
#define ASSET_LEADERLIST_NEXT_BUTTON 8
#define ASSET_LEADERLIST_HIRE_BUTTON 9
#define ASSET_LEADERLIST_POOL_BUTTON 10
#define ASSET_LEADERLIST_DISMISS_BUTTON 11
#define ASSET_LEADERLIST_RETURN_BUTTON 12
#define ASSET_LEADERLIST_SLOT_SELECTED 14
#define ASSET_LEADERLIST_SLOT_HIGHLIGHTED 15
#define ASSET_LEADER_PORTRAITS 21
#define ASSET_LEADERLIST_SKILLS 88
#define ASSET_LEADERLIST_SHIP_IMAGES 115
#define ASSET_LEADERLIST_STAR_IMAGES 130
#define ASSET_LEADER_DARK_PORTRAITS 143
#define ASSET_SKILL_ICONS 88

#define LEADER_LIST_SLOT_X 7
#define LEADER_LIST_PORTRAIT_X 13
#define LEADER_LIST_FIRST_ROW 38
#define LEADER_LIST_ROW_DIST 109
#define LEADER_LIST_SLOT_WIDTH 289
#define LEADER_LIST_SLOT_HEIGHT 105
#define LEADER_LIST_INFO_X 92
#define LEADER_LIST_INFO_WIDTH 204
#define LEADER_LIST_SCROLL_WIDTH 6

static const int leaderListMinimapStarColors[MAX_PLAYERS + 1] = {
	FONT_COLOR_STAR_RED1, FONT_COLOR_STAR_YELLOW1, FONT_COLOR_STAR_GREEN1,
	FONT_COLOR_LEADERLIST_STAR_SILVER, FONT_COLOR_STAR_BLUE2,
	FONT_COLOR_STAR_BROWN1, FONT_COLOR_STAR_PURPLE1,
	FONT_COLOR_STAR_ORANGE1, FONT_COLOR_LEADERLIST_STAR_NEUTRAL
};

static const uint8_t leaderListScrollTexture[LEADER_LIST_SCROLL_WIDTH * 3] = {
	RGB(0x3c3cd4), RGB(0x1c1ca4), RGB(0x1c1ca4), RGB(0x1c1ca4),
	RGB(0x1c1ca4), RGB(0x00006c),
};

static const char *skillFormatStrings[MAX_SKILL_TYPES][MAX_COMMON_SKILLS] = {
	{
		"%d%%", "%+d", "%+d", "%+dBC", "%+dBC", "%+d", "%+d", "%+d%%",
		"%+d%%", "%+d%%"
	},
	{
		"%+d%%", "%+d", "%+d", "%+d", "%+d", "%+d", "%+d", "%+d"
	},
	{
		"%+d%%", "%+d%%", "%+d%%", "%+d", "%+d%%", "%+d%%", "%+d%%",
		"%+d%%", "%+d"
	}
};

LeaderListView::LeaderListView(GameState *game, int activePlayer) :
	_game(game), _panelChoice(NULL), _starSystem(NULL), _grid(NULL),
	_scroll(NULL), _scrollUp(NULL), _scrollDown(NULL), _minimap(NULL),
	_minimapLabel(NULL), _selectionLabel(NULL),
	_activePlayer(activePlayer), _curLeader(-1), _selLeader(-1),
	_curStar(-1), _scrollgrab(0), _adminCount(0), _captainCount(0),
	_curFleet(NULL) {

	ImageAsset palImg;
	unsigned i, type;
	const uint8_t *pal;

	palImg = gameAssets->getImage(GALAXY_ARCHIVE, ASSET_GALAXY_GUI);
	pal = palImg->palette();
	_bg = gameAssets->getImage(LEADER_LIST_ARCHIVE, ASSET_LEADERLIST_BG,
		pal);

	for (i = 0; i < LEADER_COUNT; i++) {
		_leaderImg[i] = gameAssets->getImage(LEADER_LIST_ARCHIVE,
			ASSET_LEADER_PORTRAITS + i, pal);
		_leaderDarkImg[i] = gameAssets->getImage(LEADER_LIST_ARCHIVE,
			ASSET_LEADER_DARK_PORTRAITS + i, pal);

		if (_game->_leaders[i].playerIndex != _activePlayer) {
			continue;
		}

		type = _game->_leaders[i].type;

		if (type == LEADER_TYPE_ADMIN &&
			_adminCount < MAX_HIRED_LEADERS) {
			_admins[_adminCount++] = i;
		} else if (type == LEADER_TYPE_CAPTAIN &&
			_captainCount < MAX_HIRED_LEADERS) {
			_captains[_captainCount++] = i;
		}
	}

	for (i = 0; i < MAX_SKILLS; i++) {
		_skillImg[i] = gameAssets->getImage(LEADER_LIST_ARCHIVE,
			ASSET_LEADERLIST_SKILLS + i, pal);
	}

	i = _game->_players[_activePlayer].homePlayerId;
	_curStar = _game->_planets[i].star;
	initWidgets();
	changePanel(0, 0, 0);
	starSelectionChanged(0, 0, 0);
}

void LeaderListView::initWidgets(void) {
	ImageAsset img;
	unsigned i;
	Widget *w;
	const uint8_t *pal = _bg->palette();

	for (i = 0; i < MAX_HIRED_LEADERS; i++) {
		w = createWidget(LEADER_LIST_SLOT_X,
			LEADER_LIST_FIRST_ROW + i * LEADER_LIST_ROW_DIST,
			LEADER_LIST_SLOT_WIDTH, LEADER_LIST_SLOT_HEIGHT);
		w->setMouseOverCallback(GuiMethod(*this,
			&LeaderListView::highlightSlot, i));
		w->setMouseOutCallback(GuiMethod(*this,
			&LeaderListView::highlightSlot, -1));
		w->setMouseUpCallback(MBUTTON_LEFT, GuiMethod(*this,
			&LeaderListView::selectSlot, i));
		w->setMouseUpCallback(MBUTTON_RIGHT, GuiMethod(*this,
			&LeaderListView::showSlotHelp, i));
	}

	_panelChoice = new ChoiceWidget(7, 10, 296, 23, 2);
	addWidget(_panelChoice);
	_panelChoice->setChoiceButton(0, 0, 0, 150, 23, LEADER_LIST_ARCHIVE,
		ASSET_LEADERLIST_ADMINS_PANEL, pal, 1);
	_panelChoice->setChoiceButton(1, 153, 0, 140, 23, LEADER_LIST_ARCHIVE,
		ASSET_LEADERLIST_CAPTAINS_PANEL, pal, 1);
	_panelChoice->setValueChangeCallback(GuiMethod(*this,
		&LeaderListView::changePanel));
	_panelChoice->button(0)->setMouseUpCallback(MBUTTON_RIGHT,
		GuiMethod(*this, &LeaderListView::showHelp,
		HELP_LEADERLIST_ADMINS_BUTTON));
	_panelChoice->button(1)->setMouseUpCallback(MBUTTON_RIGHT,
		GuiMethod(*this, &LeaderListView::showHelp,
		HELP_LEADERLIST_CAPTAINS_BUTTON));

	img = gameAssets->getImage(LEADER_LIST_ARCHIVE,
		ASSET_LEADERLIST_STARSYSTEM_BG, pal);
	_starSystem = new StarSystemWidget(300, 12, 329, 189, _game,
		_activePlayer, _curStar, (Image*)img);
	addWidget(_starSystem);
	_starSystem->setMouseUpCallback(MBUTTON_RIGHT, GuiMethod(*this,
		&LeaderListView::showHelp, HELP_LEADERLIST_STAR_SYSTEM));

	img = gameAssets->getImage(LEADER_LIST_ARCHIVE,
		ASSET_LEADERLIST_SLOT_SELECTED, pal);
	_grid = new ShipGridWidget(this, 301, 18, 3, 5, 0, 0, _game,
		_activePlayer, (Image*)img, NULL, 0);
	addWidget(_grid);
	_grid->setSelectionMode(0);
	_grid->setShipHighlightCallback(GuiMethod(*this,
		&LeaderListView::shipHighlightChanged));
	_grid->setSelectionChangeCallback(GuiMethod(*this,
		&LeaderListView::shipSelectionChanged));
	_grid->setMouseUpCallback(MBUTTON_RIGHT, GuiMethod(*this,
		&LeaderListView::showHelp, HELP_LEADERLIST_SHIP_GRID));

	_scroll = new ScrollBarWidget(615, 47, LEADER_LIST_SCROLL_WIDTH, 119,
		3, 3, leaderListScrollTexture);
	addWidget(_scroll);
	_scroll->setBeginScrollCallback(GuiMethod(*this,
		&LeaderListView::handleBeginScroll));
	_scroll->setScrollCallback(GuiMethod(*this,
		&LeaderListView::handleScroll));
	_scroll->setEndScrollCallback(GuiMethod(*this,
		&LeaderListView::handleEndScroll));
	_scroll->setMouseUpCallback(MBUTTON_RIGHT,
		GuiMethod(*this, &LeaderListView::showHelp,
		HELP_LEADERLIST_SCROLLBAR));

	_scrollUp = createWidget(613, 22, 10, 19);
	_scrollUp->setMouseUpCallback(MBUTTON_LEFT,
		GuiMethod(*_scroll, &ScrollBarWidget::scrollMinus));
	_scrollUp->setClickSprite(MBUTTON_LEFT, LEADER_LIST_ARCHIVE,
		ASSET_LEADERLIST_SCROLL_UP_BUTTON, pal, 1);
	_scrollUp->setMouseUpCallback(MBUTTON_RIGHT,
		GuiMethod(*this, &LeaderListView::showHelp,
		HELP_LEADERLIST_SCROLLBAR));

	_scrollDown = createWidget(613, 170, 11, 21);
	_scrollDown->setMouseUpCallback(MBUTTON_LEFT,
		GuiMethod(*_scroll, &ScrollBarWidget::scrollPlus));
	_scrollDown->setClickSprite(MBUTTON_LEFT, LEADER_LIST_ARCHIVE,
		ASSET_LEADERLIST_SCROLL_DOWN_BUTTON, pal, 1);
	_scrollDown->setMouseUpCallback(MBUTTON_RIGHT,
		GuiMethod(*this, &LeaderListView::showHelp,
		HELP_LEADERLIST_SCROLLBAR));

	_minimap = new GalaxyMinimapWidget(306, 235, 318, 169, _game,
		_activePlayer, LEADER_LIST_ARCHIVE,
		ASSET_LEADERLIST_STAR_IMAGES, ASSET_LEADERLIST_SHIP_IMAGES,
		pal);
	addWidget(_minimap);
	_minimap->setStarFilter(SELFILTER_OWNED, SELFILTER_ANY);
	_minimap->selectStar(_curStar);
	_minimap->setFleetHighlightCallback(GuiMethod(*this,
		&LeaderListView::fleetHighlightChanged));
	_minimap->setFleetSelectCallback(GuiMethod(*this,
		&LeaderListView::fleetSelectionChanged));
	_minimap->setStarHighlightCallback(GuiMethod(*this,
		&LeaderListView::starHighlightChanged));
	_minimap->setStarSelectCallback(GuiMethod(*this,
		&LeaderListView::starSelectionChanged));
	_minimap->setMouseUpCallback(MBUTTON_RIGHT, GuiMethod(*this,
		&LeaderListView::showSelectionHelp, 3));

	w = createWidget(313, 441, 74, 27);
	w->setMouseUpCallback(MBUTTON_LEFT, GuiMethod(*this,
		&LeaderListView::clickHireButton));
	w->setClickSprite(MBUTTON_LEFT, LEADER_LIST_ARCHIVE,
		ASSET_LEADERLIST_HIRE_BUTTON, pal, 1);
	w->setMouseUpCallback(MBUTTON_RIGHT, GuiMethod(*this,
		&LeaderListView::showHelp, HELP_LEADERLIST_HIRE));

	w = createWidget(388, 441, 74, 27);
	w->setMouseUpCallback(MBUTTON_LEFT, GuiMethod(*this,
		&LeaderListView::clickPoolButton));
	w->setClickSprite(MBUTTON_LEFT, LEADER_LIST_ARCHIVE,
		ASSET_LEADERLIST_POOL_BUTTON, pal, 1);
	w->setMouseUpCallback(MBUTTON_RIGHT, GuiMethod(*this,
		&LeaderListView::showHelp, HELP_LEADERLIST_POOL));

	w = createWidget(463, 441, 74, 27);
	w->setMouseUpCallback(MBUTTON_LEFT, GuiMethod(*this,
		&LeaderListView::clickDismissButton));
	w->setClickSprite(MBUTTON_LEFT, LEADER_LIST_ARCHIVE,
		ASSET_LEADERLIST_DISMISS_BUTTON, pal, 1);
	w->setMouseUpCallback(MBUTTON_RIGHT, GuiMethod(*this,
		&LeaderListView::showHelp, HELP_LEADERLIST_DISMISS));

	w = createWidget(538, 441, 80, 27);
	w->setMouseUpCallback(MBUTTON_LEFT, GuiMethod(*this,
		&LeaderListView::clickReturn));
	w->setClickSprite(MBUTTON_LEFT, LEADER_LIST_ARCHIVE,
		ASSET_LEADERLIST_RETURN_BUTTON, pal, 1);
	w->setMouseUpCallback(MBUTTON_RIGHT, GuiMethod(*this,
		&LeaderListView::showHelp, HELP_LEADERLIST_RETURN));

	w = createWidget(327, 205, 35, 21);
	w->setMouseUpCallback(MBUTTON_LEFT, GuiMethod(*this,
		&LeaderListView::clickSelectPrevious));
	w->setClickSprite(MBUTTON_LEFT, LEADER_LIST_ARCHIVE,
		ASSET_LEADERLIST_PREV_BUTTON, pal, 1);
	w->setMouseUpCallback(MBUTTON_RIGHT, GuiMethod(*this,
		&LeaderListView::showSelectionHelp, 1));

	w = createWidget(568, 205, 35, 21);
	w->setMouseUpCallback(MBUTTON_LEFT, GuiMethod(*this,
		&LeaderListView::clickSelectNext));
	w->setClickSprite(MBUTTON_LEFT, LEADER_LIST_ARCHIVE,
		ASSET_LEADERLIST_NEXT_BUTTON, pal, 1);
	w->setMouseUpCallback(MBUTTON_RIGHT, GuiMethod(*this,
		&LeaderListView::showSelectionHelp, 2));

	_minimapLabel = new LabelWidget(364, 412, 204, 22);
	addWidget(_minimapLabel);
	_minimapLabel->setMouseUpCallback(MBUTTON_RIGHT, GuiMethod(*this,
		&LeaderListView::showHelp, HELP_LEADERLIST_MINIMAP_LABEL));

	_selectionLabel = new LabelWidget(364, 205, 204, 22);
	addWidget(_selectionLabel);
	_selectionLabel->setMouseUpCallback(MBUTTON_RIGHT, GuiMethod(*this,
		&LeaderListView::showSelectionHelp, 0));
}

int LeaderListView::getLeaderID(int slot) const {
	if (slot < 0) {
		return -1;
	}

	if (_panelChoice->value()) {
		return slot >= (int)_captainCount ? -1 : _captains[slot];
	} else {
		return slot >= (int)_adminCount ? -1 : _admins[slot];
	}
}

unsigned LeaderListView::drawSkills(unsigned x, unsigned y, const Leader *lptr,
	unsigned base, unsigned count, unsigned color) {

	unsigned i, idx, x2, level;
	Font *fnt;
	const char *str;
	StringBuffer buf;

	fnt = gameFonts->getFont(FONTSIZE_MEDIUM);

	for (i = 0; i < count; i++) {
		level = lptr->hasSkill(base + i);

		if (!level) {
			continue;
		}

		idx = base + i;
		str = skillFormatStrings[SKILLTYPE(idx)][idx & SKILLCODE_MASK];
		buf.printf(str, lptr->skillBonus(base + i));
		str = Leader::skillName(base + i, level > 1);
		idx = Leader::skillNum(base + i);
		_skillImg[idx]->draw(x + 2, y);
		fnt->renderText(x + 24, y + 4, color, str, OUTLINE_FULL);
		x2 = x + LEADER_LIST_INFO_WIDTH - 3;
		x2 -= fnt->textWidth(buf.c_str());
		fnt->renderText(x2, y + 4, color, buf.c_str(), OUTLINE_FULL);
		y += _skillImg[idx]->height() + 1;
	}

	return y;
}

void LeaderListView::redraw(unsigned curtick) {
	unsigned i, x, y, count, skillbase, skillcount, *idlist;
	unsigned color, color2;
	int location;
	const Leader *ptr;
	Font *fnt, *smallfnt;
	const Image *img;
	const char *str;
	StringBuffer buf;

	clearScreen();
	_bg->draw(0, 0);
	fnt = gameFonts->getFont(FONTSIZE_MEDIUM);
	smallfnt = gameFonts->getFont(FONTSIZE_SMALL);

	if (_panelChoice->value()) {
		idlist = _captains;
		count = _captainCount;
		skillbase = CAPTAIN_SKILLS_TYPE;
		skillcount = MAX_CAPTAIN_SKILLS;
	} else {
		idlist = _admins;
		count = _adminCount;
		skillbase = ADMIN_SKILLS_TYPE;
		skillcount = MAX_ADMIN_SKILLS;
	}

	for (i = 0; i < count; i++) {
		ptr = _game->_leaders + idlist[i];
		color = color2 = FONT_COLOR_LEADERLIST_NORMAL;

		if (_curLeader == (int)i || _selLeader == (int)i) {
			color = FONT_COLOR_LEADERLIST_BRIGHT;
			color2 = FONT_COLOR_LEADERLIST_BRIGHT_NAME;
		}

		x = LEADER_LIST_INFO_X;
		y = LEADER_LIST_FIRST_ROW + i * LEADER_LIST_ROW_DIST;
		buf = ptr->rank();
		buf += " ";
		buf += ptr->name;
		fnt->centerText(x + LEADER_LIST_INFO_WIDTH / 2, y, color2,
			buf.c_str(), OUTLINE_FULL);

		x = LEADER_LIST_PORTRAIT_X;

		if (ptr->status == LeaderState::ForHire || ptr->eta) {
			img = (const Image*)_leaderDarkImg[ptr->picture];
		} else {
			img = (const Image*)_leaderImg[ptr->picture];
		}

		img->draw(x, y);
		color2 = color;

		switch (ptr->status) {
		case LeaderState::Idle:
			str = gameLang->hstrings(HSTR_OFFICER_IN_POOL);
			break;

		case LeaderState::Working:
			location = ptr->location;

			if (ptr->eta) {
				str = gameLang->hstrings(HSTR_FLEET_OFFICER_ETA);
				buf.printf(str, ptr->eta);
				drawETA(x + img->width() / 2,
					y - 3 + img->height() / 2,
					FONT_COLOR_FLEETLIST_SPECDAMAGE,
					buf.c_str());
			}

			if (location < 0) {
				str = NULL;
			} else if (ptr->type == LEADER_TYPE_CAPTAIN) {
				str = _game->_ships[location].design.name;
			} else {
				str = _game->_starSystems[location].name;
			}

			break;

		case LeaderState::Unassigned:
			str = gameLang->hstrings(HSTR_OFFICER_UNASSIGNED);
			break;

		case LeaderState::ForHire:
			str = gameLang->hstrings(HSTR_OFFICER_FOR_HIRE_TIME);
			buf.printf(str, 30 - ptr->eta);
			str = buf.c_str();
			color2 = FONT_COLOR_FLEETLIST_SPECDAMAGE;
			break;

		default:
			str = NULL;
			break;
		}

		if (str) {
			smallfnt->centerText(x + img->width() / 2,
				y + img->height() + 5, color2, str,
				OUTLINE_FULL, 2);
		}

		// TODO: draw salary/hire price

		x = LEADER_LIST_INFO_X;
		y += fnt->height() - 1;
		y = drawSkills(x, y, ptr, skillbase, skillcount, color);
		drawSkills(x, y, ptr, COMMON_SKILLS_TYPE, MAX_COMMON_SKILLS,
			color);
	}

	redrawWidgets(0, 0, curtick);
	redrawWindows(curtick);
}

void LeaderListView::handleMouseMove(int x, int y, unsigned buttons) {
	if (_scrollgrab) {
		_scroll->handleMouseMove(x, y, buttons);
		return;
	}

	GuiView::handleMouseMove(x, y, buttons);
}

void LeaderListView::handleMouseUp(int x, int y, unsigned button) {
	if (_scrollgrab) {
		_scroll->handleMouseUp(x, y, button);
		return;
	}

	GuiView::handleMouseUp(x, y, button);
}

void LeaderListView::showHelp(int x, int y, int arg) {
	new MessageBoxWindow(this, arg, _bg->palette());
}

void LeaderListView::showSelectionHelp(int x, int y, int arg) {
	unsigned idx;

	if (_panelChoice->value()) {
		idx = HELP_LEADERLIST_SHIP_DESCRIPTION;
	} else {
		idx = HELP_LEADERLIST_STAR_DESCRIPTION;
	}

	new MessageBoxWindow(this, idx + arg, _bg->palette());
}

void LeaderListView::showSlotHelp(int x, int y, int arg) {
	unsigned i, base, skillcount, total = 0, skill_list[MAX_SKILLS];
	int idx = getLeaderID(arg);
	Leader *ptr = idx >= 0 ? _game->_leaders + idx : NULL;
	Font *fnt;
	const char *str;
	StringBuffer buf, namebuf;

	if (ptr) {
		namebuf.printf("%s %s", ptr->rank(), ptr->name);
	}

	if (x < LEADER_LIST_INFO_X) {
		if (!ptr) {
			return;
		}

		if (ptr->status == LeaderState::Idle) {
			str = gameLang->hstrings(HSTR_OFFICER_LOCATION_POOL);
			buf.printf(str, namebuf.c_str());
			new ErrorWindow(this, buf.c_str());
			return;
		} else if (ptr->status != LeaderState::Working ||
			ptr->location < 0) {
			str = gameLang->hstrings(HSTR_OFFICER_LOCATION_NONE);
			buf.printf(str, namebuf.c_str());
			new ErrorWindow(this, buf.c_str());
			return;
		}

		if (_panelChoice->value()) {
			const Ship *sptr = _game->_ships + ptr->location;

			if (sptr->status == ShipState::InRefit) {
				i = HSTR_OFFICER_LOCATION_REFIT;
				idx = sptr->getStarID();
				buf.printf(gameLang->hstrings(i),
					namebuf.c_str(), sptr->design.name,
					_game->_starSystems[idx].name);
				new ErrorWindow(this, buf.c_str());
				return;
			} else if (!sptr->isActive()) {
				i = HSTR_OFFICER_LOCATION_MISSING;
				buf.printf(gameLang->hstrings(i),
					namebuf.c_str());
				new ErrorWindow(this, buf.c_str());
				return;
			}

			_minimap->selectFleet(_game->findFleet(sptr));
			fleetSelectionChanged(x, y, 0);
		} else {
			_minimap->selectStar(ptr->location);
			starSelectionChanged(x, y, 0);
		}

		return;
	}

	if (ptr) {
		if (_panelChoice->value()) {
			base = CAPTAIN_SKILLS_TYPE;
			skillcount = MAX_CAPTAIN_SKILLS;
		} else {
			base = ADMIN_SKILLS_TYPE;
			skillcount = MAX_ADMIN_SKILLS;
		}

		for (i = 0, total = 0; i < skillcount; i++) {
			if (ptr->hasSkill(base + i)) {
				skill_list[total++] = base + i;
			}
		}

		base = COMMON_SKILLS_TYPE;

		for (i = 0; i < MAX_COMMON_SKILLS; i++) {
			if (ptr->hasSkill(base + i)) {
				skill_list[total++] = base + i;
			}
		}

		fnt = gameFonts->getFont(FONTSIZE_MEDIUM);
		y -= LEADER_LIST_FIRST_ROW + arg * LEADER_LIST_ROW_DIST;
		y -= fnt->height() - 1;

		if (y < 0) {
			return;
		}

		i = y / (_skillImg[0]->height() + 1);

		if (i < total) {
			idx = HSTR_OFFICER_DEFINITE_ARTICLE;
			namebuf.append(gameLang->hstrings(idx));
			namebuf.append(ptr->title);
			namebuf.append(",");
			idx = ptr->skillNum(skill_list[i]);
			buf.printf(gameLang->skilldesc(idx),
				namebuf.c_str(),
				abs(ptr->skillBonus(skill_list[i])));
			new MessageBoxWindow(this, gameLang->skillname(idx),
				buf.c_str());
			return;
		}
	}

	if (_panelChoice->value()) {
		idx = HELP_LEADERLIST_CAPTAIN_LIST;
	} else {
		idx = HELP_LEADERLIST_ADMIN_LIST;
	}

	new MessageBoxWindow(this, idx, _bg->palette());
}

void LeaderListView::changePanel(int x, int y, int arg) {
	unsigned val = _panelChoice->value();

	_curLeader = -1;
	_selLeader = -1;
	_starSystem->hide(val);
	_grid->hide(!val);
	_scroll->hide(!val);
	_scrollUp->hide(!val);
	_scrollDown->hide(!val);

	if (val) {
		_minimap->selectStar(-1);
		_minimap->selectFleet(_curFleet);
		fleetSelectionChanged(x, y, 0);
	} else {
		_minimap->selectFleet(NULL);
		_minimap->selectStar(_curStar);
		starSelectionChanged(x, y, 0);
	}
}

void LeaderListView::highlightSlot(int x, int y, int arg) {
	if (getLeaderID(arg) < 0) {
		arg = -1;
	}

	_curLeader = arg;
}

void LeaderListView::selectSlot(int x, int y, int arg) {
	int idx, location;
	Leader *ptr = NULL;

	idx = getLeaderID(arg);

	if (idx < 0) {
		return;
	}

	ptr = _game->_leaders + idx;

	if (ptr->status == LeaderState::ForHire) {
		// TODO: Ask whether to hire leader
		new MessageBoxWindow(this, "Not implemented yet");
		return;
	}

	if (_panelChoice->value()) {
		location = _grid->selectedShipID();

		if (location < 0) {
			_selLeader = (_selLeader == arg) ? -1 : arg;
			return;
		}

		// TODO: Ask whether to assign leader to ship
		new MessageBoxWindow(this, "Not implemented yet");
		return;
	} else {
		location = _minimap->selectedStar();

		if (ptr->status == LeaderState::Working &&
			ptr->location == location) {
			return;
		}

		// TODO: Ask whether to assign leader to star system
		new MessageBoxWindow(this, "Not implemented yet");
		return;
	}
}

void LeaderListView::fleetHighlightChanged(int x, int y, int arg) {
	unsigned owner, color;
	const Fleet *f = _minimap->highlightedFleet();
	StringBuffer buf;

	if (!f) {
		_minimapLabel->clear();
		return;
	}

	owner = f->getOwner();
	color = FONT_COLOR_FLEETLIST_FLEET_RED;
	fleetSummary(buf, *f);

	if (owner > MAX_PLAYERS) {
		_minimapLabel->setText(buf.c_str(), FONTSIZE_BIG,
			FONT_COLOR_FLEETLIST_FLEET_MONSTER, OUTLINE_FULL,
			ALIGN_CENTER);
		return;
	}

	if (owner < MAX_PLAYERS) {
		color += f->getColor();
	}

	_minimapLabel->setText(buf.c_str(), FONTSIZE_SMALLER, color,
		OUTLINE_FULL, ALIGN_CENTER);

}

void LeaderListView::fleetSelectionChanged(int x, int y, int arg) {
	Fleet *f;

	if (!_panelChoice->value()) {
		_minimap->selectFleet(NULL);
		return;
	}

	f = _minimap->selectedFleet();
	_curFleet = f;
	_grid->setFleet(f, -1);
	updateScrollbar();
	shipHighlightChanged(x, y, 0);
}

void LeaderListView::shipHighlightChanged(int x, int y, int arg) {
	const Ship *sptr = _grid->highlightedShip();

	if (!sptr) {
		_selectionLabel->clear();
		return;
	}

	if (sptr->officer >= 0) {
		StringBuffer buf;

		buf.printf("%s (%s)", sptr->design.name,
			_game->_leaders[sptr->officer].name);
		_selectionLabel->setText(buf.c_str(), FONTSIZE_MEDIUM,
			FONT_COLOR_LEADERLIST_NORMAL, OUTLINE_NONE,
			ALIGN_CENTER);
	} else {
		_selectionLabel->setText(sptr->design.name, FONTSIZE_MEDIUM,
			FONT_COLOR_LEADERLIST_NORMAL, OUTLINE_NONE,
			ALIGN_CENTER);
	}
}

void LeaderListView::shipSelectionChanged(int x, int y, int arg) {
	const Ship *sptr = _grid->selectedShip();
	const char *str;

	if (sptr && sptr->design.type != ShipType::COMBAT_SHIP) {
		_grid->selectNone();
		str =  gameLang->hstrings(HSTR_OFFICER_ASSIGN_NONCOMBAT);
		new ErrorWindow(this, str);
		return;
	}

	if (_selLeader >= 0 && sptr) {
		_selLeader = -1;
		_grid->selectNone();
		new MessageBoxWindow(this, "Not implemented yet");
	}
}

void LeaderListView::starHighlightChanged(int x, int y, int arg) {
	unsigned color;
	int idx, star = _minimap->highlightedStar();
	const Star *sptr;

	if (star < 0) {
		_minimapLabel->clear();
		return;
	}

	sptr = _game->_starSystems + star;

	if (sptr->owner < 0) {
		color = MAX_PLAYERS;
	} else {
		color = _game->_players[sptr->owner].color;
	}

	color = leaderListMinimapStarColors[color];
	idx = sptr->officerIndex[_activePlayer];

	if (idx < 0) {
		_minimapLabel->setText(sptr->name, FONTSIZE_MEDIUM, color,
			OUTLINE_NONE, ALIGN_CENTER);
	} else {
		StringBuffer buf;

		buf.printf("%s (%s)", sptr->name, _game->_leaders[idx].name);
		_minimapLabel->setText(buf.c_str(), FONTSIZE_MEDIUM, color,
			OUTLINE_NONE, ALIGN_CENTER);
	}
}

void LeaderListView::starSelectionChanged(int x, int y, int arg) {
	int idx, star = _minimap->selectedStar();

	if (_panelChoice->value()) {
		_minimap->selectStar(-1);
		return;
	}

	if (star < 0) {
		return;
	}

	_curStar = star;
	_starSystem->setStar(star);
	idx = _game->_starSystems[star].officerIndex[_activePlayer];

	if (idx >= 0) {
		StringBuffer buf;

		buf.printf("%s (%s)", _game->_starSystems[star].name,
			_game->_leaders[idx].name);
		_selectionLabel->setText(buf.c_str(), FONTSIZE_MEDIUM,
			FONT_COLOR_LEADERLIST_NORMAL, OUTLINE_NONE,
			ALIGN_CENTER);
	} else {
		_selectionLabel->setText(_game->_starSystems[star].name,
			FONTSIZE_MEDIUM, FONT_COLOR_LEADERLIST_NORMAL,
			OUTLINE_NONE, ALIGN_CENTER);
	}
}

void LeaderListView::handleBeginScroll(int x, int y, int arg) {
	_scrollgrab = 1;
}

void LeaderListView::handleScroll(int x, int y, int arg) {
	_grid->setScroll(_scroll->position());
}

void LeaderListView::handleEndScroll(int x, int y, int arg) {
	_scrollgrab = 0;
	selectCurrentWidget(x, y, 0);
}

void LeaderListView::updateScrollbar(void) {
	unsigned cols = _grid->cols();

	_scroll->setRange((_grid->visibleShipCount() + cols - 1) / cols);
}

void LeaderListView::clickSelectPrevious(int x, int y, int arg) STUB(this)

void LeaderListView::clickSelectNext(int x, int y, int arg) STUB(this)

void LeaderListView::clickHireButton(int x, int y, int arg) STUB(this)

void LeaderListView::clickPoolButton(int x, int y, int arg) STUB(this)

void LeaderListView::clickDismissButton(int x, int y, int arg) STUB(this)

void LeaderListView::clickReturn(int x, int y, int arg) {
	exitView();
}
