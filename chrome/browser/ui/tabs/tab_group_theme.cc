// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_theme.h"

#include <array>

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"

using TP = ThemeProperties;
using TabGroupColorId = tab_groups::TabGroupColorId;

int GetTabGroupTabStripColorId(TabGroupColorId group_color_id,
                               bool active_frame) {
  static constexpr auto group_id_map =
      base::MakeFixedFlatMap<TabGroupColorId, std::array<int, 2>>({
          {TabGroupColorId::kGrey,
           {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_GREY,
            TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_GREY}},
          {TabGroupColorId::kBlue,
           {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_BLUE,
            TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_BLUE}},
          {TabGroupColorId::kRed,
           {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_RED,
            TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_RED}},
          {TabGroupColorId::kYellow,
           {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_YELLOW,
            TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_YELLOW}},
          {TabGroupColorId::kGreen,
           {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_GREEN,
            TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_GREEN}},
          {TabGroupColorId::kPink,
           {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_PINK,
            TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_PINK}},
          {TabGroupColorId::kPurple,
           {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_PURPLE,
            TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_PURPLE}},
          {TabGroupColorId::kCyan,
           {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_CYAN,
            TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_CYAN}},
          {TabGroupColorId::kOrange,
           {TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_INACTIVE_ORANGE,
            TP::COLOR_TAB_GROUP_TABSTRIP_FRAME_ACTIVE_ORANGE}},
      });

  return group_id_map.at(group_color_id)[active_frame];
}

int GetTabGroupDialogColorId(TabGroupColorId group_color_id) {
  static constexpr auto group_id_map =
      base::MakeFixedFlatMap<TabGroupColorId, int>({
          {TabGroupColorId::kGrey, TP::COLOR_TAB_GROUP_DIALOG_GREY},
          {TabGroupColorId::kBlue, TP::COLOR_TAB_GROUP_DIALOG_BLUE},
          {TabGroupColorId::kRed, TP::COLOR_TAB_GROUP_DIALOG_RED},
          {TabGroupColorId::kYellow, TP::COLOR_TAB_GROUP_DIALOG_YELLOW},
          {TabGroupColorId::kGreen, TP::COLOR_TAB_GROUP_DIALOG_GREEN},
          {TabGroupColorId::kPink, TP::COLOR_TAB_GROUP_DIALOG_PINK},
          {TabGroupColorId::kPurple, TP::COLOR_TAB_GROUP_DIALOG_PURPLE},
          {TabGroupColorId::kCyan, TP::COLOR_TAB_GROUP_DIALOG_CYAN},
          {TabGroupColorId::kOrange, TP::COLOR_TAB_GROUP_DIALOG_ORANGE},
      });

  return group_id_map.at(group_color_id);
}

ui::ColorId GetTabGroupContextMenuColorId(TabGroupColorId group_color_id) {
  static constexpr auto group_id_map =
      base::MakeFixedFlatMap<TabGroupColorId, ui::ColorId>({
          {TabGroupColorId::kGrey, kColorTabGroupContextMenuGrey},
          {TabGroupColorId::kBlue, kColorTabGroupContextMenuBlue},
          {TabGroupColorId::kRed, kColorTabGroupContextMenuRed},
          {TabGroupColorId::kYellow, kColorTabGroupContextMenuYellow},
          {TabGroupColorId::kGreen, kColorTabGroupContextMenuGreen},
          {TabGroupColorId::kPink, kColorTabGroupContextMenuPink},
          {TabGroupColorId::kPurple, kColorTabGroupContextMenuPurple},
          {TabGroupColorId::kCyan, kColorTabGroupContextMenuCyan},
          {TabGroupColorId::kOrange, kColorTabGroupContextMenuOrange},
      });

  return group_id_map.at(group_color_id);
}

int GetTabGroupContextMenuColorIdDeprecated(TabGroupColorId group_color_id) {
  static constexpr auto group_id_map =
      base::MakeFixedFlatMap<TabGroupColorId, int>({
          {TabGroupColorId::kGrey, TP::COLOR_TAB_GROUP_CONTEXT_MENU_GREY},
          {TabGroupColorId::kBlue, TP::COLOR_TAB_GROUP_CONTEXT_MENU_BLUE},
          {TabGroupColorId::kRed, TP::COLOR_TAB_GROUP_CONTEXT_MENU_RED},
          {TabGroupColorId::kYellow, TP::COLOR_TAB_GROUP_CONTEXT_MENU_YELLOW},
          {TabGroupColorId::kGreen, TP::COLOR_TAB_GROUP_CONTEXT_MENU_GREEN},
          {TabGroupColorId::kPink, TP::COLOR_TAB_GROUP_CONTEXT_MENU_PINK},
          {TabGroupColorId::kPurple, TP::COLOR_TAB_GROUP_CONTEXT_MENU_PURPLE},
          {TabGroupColorId::kCyan, TP::COLOR_TAB_GROUP_CONTEXT_MENU_CYAN},
          {TabGroupColorId::kOrange, TP::COLOR_TAB_GROUP_CONTEXT_MENU_ORANGE},
      });

  return group_id_map.at(group_color_id);
}

int GetTabGroupBookmarkColorId(tab_groups::TabGroupColorId group_color_id) {
  static constexpr auto group_id_map =
      base::MakeFixedFlatMap<TabGroupColorId, int>({
          {TabGroupColorId::kGrey, TP::COLOR_TAB_GROUP_BOOKMARK_BAR_GREY},
          {TabGroupColorId::kBlue, TP::COLOR_TAB_GROUP_BOOKMARK_BAR_BLUE},
          {TabGroupColorId::kRed, TP::COLOR_TAB_GROUP_BOOKMARK_BAR_RED},
          {TabGroupColorId::kYellow, TP::COLOR_TAB_GROUP_BOOKMARK_BAR_YELLOW},
          {TabGroupColorId::kGreen, TP::COLOR_TAB_GROUP_BOOKMARK_BAR_GREEN},
          {TabGroupColorId::kPink, TP::COLOR_TAB_GROUP_BOOKMARK_BAR_PINK},
          {TabGroupColorId::kPurple, TP::COLOR_TAB_GROUP_BOOKMARK_BAR_PURPLE},
          {TabGroupColorId::kCyan, TP::COLOR_TAB_GROUP_BOOKMARK_BAR_CYAN},
          {TabGroupColorId::kOrange, TP::COLOR_TAB_GROUP_BOOKMARK_BAR_ORANGE},
      });

  return group_id_map.at(group_color_id);
}
