// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_group_theme.h"

#include <array>

#include "base/containers/fixed_flat_map.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/color/chrome_color_id.h"

using TP = ThemeProperties;
using TabGroupColorId = tab_groups::TabGroupColorId;

ui::ColorId GetTabGroupTabStripColorId(TabGroupColorId group_color_id,
                                       bool active_frame) {
  static constexpr auto group_id_map =
      base::MakeFixedFlatMap<TabGroupColorId, std::array<int, 2>>({
          {TabGroupColorId::kGrey,
           {kColorTabGroupTabStripFrameInactiveGrey,
            kColorTabGroupTabStripFrameActiveGrey}},
          {TabGroupColorId::kBlue,
           {kColorTabGroupTabStripFrameInactiveBlue,
            kColorTabGroupTabStripFrameActiveBlue}},
          {TabGroupColorId::kRed,
           {kColorTabGroupTabStripFrameInactiveRed,
            kColorTabGroupTabStripFrameActiveRed}},
          {TabGroupColorId::kYellow,
           {kColorTabGroupTabStripFrameInactiveYellow,
            kColorTabGroupTabStripFrameActiveYellow}},
          {TabGroupColorId::kGreen,
           {kColorTabGroupTabStripFrameInactiveGreen,
            kColorTabGroupTabStripFrameActiveGreen}},
          {TabGroupColorId::kPink,
           {kColorTabGroupTabStripFrameInactivePink,
            kColorTabGroupTabStripFrameActivePink}},
          {TabGroupColorId::kPurple,
           {kColorTabGroupTabStripFrameInactivePurple,
            kColorTabGroupTabStripFrameActivePurple}},
          {TabGroupColorId::kCyan,
           {kColorTabGroupTabStripFrameInactiveCyan,
            kColorTabGroupTabStripFrameActiveCyan}},
          {TabGroupColorId::kOrange,
           {kColorTabGroupTabStripFrameInactiveOrange,
            kColorTabGroupTabStripFrameActiveOrange}},
      });

  return group_id_map.at(group_color_id)[active_frame];
}

ui::ColorId GetThumbnailTabStripTabGroupColorId(TabGroupColorId group_color_id,
                                                bool active_frame) {
  static constexpr auto group_id_map =
      base::MakeFixedFlatMap<TabGroupColorId, std::array<ui::ColorId, 2>>({
          {TabGroupColorId::kGrey,
           {kColorThumbnailTabStripTabGroupFrameInactiveGrey,
            kColorThumbnailTabStripTabGroupFrameActiveGrey}},
          {TabGroupColorId::kBlue,
           {kColorThumbnailTabStripTabGroupFrameInactiveBlue,
            kColorThumbnailTabStripTabGroupFrameActiveBlue}},
          {TabGroupColorId::kRed,
           {kColorThumbnailTabStripTabGroupFrameInactiveRed,
            kColorThumbnailTabStripTabGroupFrameActiveRed}},
          {TabGroupColorId::kYellow,
           {kColorThumbnailTabStripTabGroupFrameInactiveYellow,
            kColorThumbnailTabStripTabGroupFrameActiveYellow}},
          {TabGroupColorId::kGreen,
           {kColorThumbnailTabStripTabGroupFrameInactiveGreen,
            kColorThumbnailTabStripTabGroupFrameActiveGreen}},
          {TabGroupColorId::kPink,
           {kColorThumbnailTabStripTabGroupFrameInactivePink,
            kColorThumbnailTabStripTabGroupFrameActivePink}},
          {TabGroupColorId::kPurple,
           {kColorThumbnailTabStripTabGroupFrameInactivePurple,
            kColorThumbnailTabStripTabGroupFrameActivePurple}},
          {TabGroupColorId::kCyan,
           {kColorThumbnailTabStripTabGroupFrameInactiveCyan,
            kColorThumbnailTabStripTabGroupFrameActiveCyan}},
          {TabGroupColorId::kOrange,
           {kColorThumbnailTabStripTabGroupFrameInactiveOrange,
            kColorThumbnailTabStripTabGroupFrameActiveOrange}},
      });

  return group_id_map.at(group_color_id)[active_frame];
}

ui::ColorId GetTabGroupDialogColorId(TabGroupColorId group_color_id) {
  static constexpr auto group_id_map =
      base::MakeFixedFlatMap<TabGroupColorId, int>({
          {TabGroupColorId::kGrey, kColorTabGroupDialogGrey},
          {TabGroupColorId::kBlue, kColorTabGroupDialogBlue},
          {TabGroupColorId::kRed, kColorTabGroupDialogRed},
          {TabGroupColorId::kYellow, kColorTabGroupDialogYellow},
          {TabGroupColorId::kGreen, kColorTabGroupDialogGreen},
          {TabGroupColorId::kPink, kColorTabGroupDialogPink},
          {TabGroupColorId::kPurple, kColorTabGroupDialogPurple},
          {TabGroupColorId::kCyan, kColorTabGroupDialogCyan},
          {TabGroupColorId::kOrange, kColorTabGroupDialogOrange},
      });

  return group_id_map.at(group_color_id);
}

ui::ColorId GetSavedTabGroupForegroundColorId(TabGroupColorId group_color_id) {
  static constexpr auto group_id_map =
      base::MakeFixedFlatMap<TabGroupColorId, int>({
          {TabGroupColorId::kGrey, kColorSavedTabGroupForegroundGrey},
          {TabGroupColorId::kBlue, kColorSavedTabGroupForegroundBlue},
          {TabGroupColorId::kRed, kColorSavedTabGroupForegroundRed},
          {TabGroupColorId::kYellow, kColorSavedTabGroupForegroundYellow},
          {TabGroupColorId::kGreen, kColorSavedTabGroupForegroundGreen},
          {TabGroupColorId::kPink, kColorSavedTabGroupForegroundPink},
          {TabGroupColorId::kPurple, kColorSavedTabGroupForegroundPurple},
          {TabGroupColorId::kCyan, kColorSavedTabGroupForegroundCyan},
          {TabGroupColorId::kOrange, kColorSavedTabGroupForegroundOrange},
      });

  return group_id_map.at(group_color_id);
}

ui::ColorId GetSavedTabGroupOutlineColorId(TabGroupColorId group_color_id) {
  static constexpr auto group_id_map =
      base::MakeFixedFlatMap<TabGroupColorId, int>({
          {TabGroupColorId::kGrey, kColorSavedTabGroupOutlineGrey},
          {TabGroupColorId::kBlue, kColorSavedTabGroupOutlineBlue},
          {TabGroupColorId::kRed, kColorSavedTabGroupOutlineRed},
          {TabGroupColorId::kYellow, kColorSavedTabGroupOutlineYellow},
          {TabGroupColorId::kGreen, kColorSavedTabGroupOutlineGreen},
          {TabGroupColorId::kPink, kColorSavedTabGroupOutlinePink},
          {TabGroupColorId::kPurple, kColorSavedTabGroupOutlinePurple},
          {TabGroupColorId::kCyan, kColorSavedTabGroupOutlineCyan},
          {TabGroupColorId::kOrange, kColorSavedTabGroupOutlineOrange},
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

ui::ColorId GetTabGroupBookmarkColorId(
    tab_groups::TabGroupColorId group_color_id) {
  static constexpr auto group_id_map =
      base::MakeFixedFlatMap<TabGroupColorId, int>({
          {TabGroupColorId::kGrey, kColorTabGroupBookmarkBarGrey},
          {TabGroupColorId::kBlue, kColorTabGroupBookmarkBarBlue},
          {TabGroupColorId::kRed, kColorTabGroupBookmarkBarRed},
          {TabGroupColorId::kYellow, kColorTabGroupBookmarkBarYellow},
          {TabGroupColorId::kGreen, kColorTabGroupBookmarkBarGreen},
          {TabGroupColorId::kPink, kColorTabGroupBookmarkBarPink},
          {TabGroupColorId::kPurple, kColorTabGroupBookmarkBarPurple},
          {TabGroupColorId::kCyan, kColorTabGroupBookmarkBarCyan},
          {TabGroupColorId::kOrange, kColorTabGroupBookmarkBarOrange},
      });

  return group_id_map.at(group_color_id);
}
