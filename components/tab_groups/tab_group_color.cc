// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tab_groups/tab_group_color.h"

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "components/tab_groups/tab_group_id.h"
#include "ui/base/l10n/l10n_util.h"

namespace tab_groups {

const ColorLabelMap& GetTabGroupColorLabelMap() {
  static const base::NoDestructor<ColorLabelMap> kTabGroupColors(
      {{TabGroupColorId::kGrey,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_GREY)},
       {TabGroupColorId::kBlue,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_BLUE)},
       {TabGroupColorId::kRed,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_RED)},
       {TabGroupColorId::kYellow,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_YELLOW)},
       {TabGroupColorId::kGreen,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_GREEN)},
       {TabGroupColorId::kPink,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_PINK)},
       {TabGroupColorId::kPurple,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_PURPLE)},
       {TabGroupColorId::kCyan,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_CYAN)},
       {TabGroupColorId::kOrange,
        l10n_util::GetStringUTF16(IDS_TAB_GROUP_COLOR_ORANGE)}});
  return *kTabGroupColors;
}

TabGroupColorId GetNextColor(const std::vector<TabGroupColorId>& used_colors) {
  // Count the number of times each available color is used.
  std::map<TabGroupColorId, int> color_usage_counts;
  for (const auto& id_color_pair : GetTabGroupColorLabelMap()) {
    color_usage_counts[id_color_pair.first] = 0;
  }
  for (const auto& color : used_colors) {
    color_usage_counts[color]++;
  }

  // Find the next least-used color.
  TabGroupColorId next_color = color_usage_counts.begin()->first;
  int min_usage_count = color_usage_counts.begin()->second;
  for (const auto& color_usage_pair : color_usage_counts) {
    if (color_usage_pair.second < min_usage_count) {
      next_color = color_usage_pair.first;
      min_usage_count = color_usage_pair.second;
    }
  }
  return next_color;
}

}  // namespace tab_groups
