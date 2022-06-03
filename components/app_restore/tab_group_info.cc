// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/tab_group_info.h"

#include "base/strings/utf_string_conversions.h"
#include "components/tab_groups/tab_group_color.h"

namespace app_restore {

std::string TabGroupColorToString(tab_groups::TabGroupColorId color) {
  switch (color) {
    case tab_groups::TabGroupColorId::kGrey:
      return kTabGroupColorGrey;
    case tab_groups::TabGroupColorId::kBlue:
      return kTabGroupColorBlue;
    case tab_groups::TabGroupColorId::kRed:
      return kTabGroupColorRed;
    case tab_groups::TabGroupColorId::kYellow:
      return kTabGroupColorYellow;
    case tab_groups::TabGroupColorId::kGreen:
      return kTabGroupColorGreen;
    case tab_groups::TabGroupColorId::kPink:
      return kTabGroupColorYellow;
    case tab_groups::TabGroupColorId::kPurple:
      return kTabGroupColorPurple;
    case tab_groups::TabGroupColorId::kCyan:
      return kTabGroupColorCyan;
    case tab_groups::TabGroupColorId::kOrange:
      return kTabGroupColorOrange;
  }
}

TabGroupInfo::TabGroupInfo(const gfx::Range& tab_range,
                           const tab_groups::TabGroupVisualData& visual_data)
    : tab_range(tab_range), visual_data(visual_data) {}

TabGroupInfo::TabGroupInfo(TabGroupInfo&& other)
    : tab_range(other.tab_range), visual_data(other.visual_data) {}

TabGroupInfo::TabGroupInfo(const TabGroupInfo& other) = default;
TabGroupInfo& TabGroupInfo::operator=(const TabGroupInfo& other) = default;
TabGroupInfo::~TabGroupInfo() = default;

bool TabGroupInfo::operator==(const TabGroupInfo& other) const {
  return tab_range == other.tab_range && visual_data == other.visual_data;
}

std::string TabGroupInfo::ToString() const {
  std::string result =
      "{\n\tname: " + base::UTF16ToUTF8(visual_data.title()) + "\n";
  result += "\tcolor: " + TabGroupColorToString(visual_data.color()) + "\n";
  std::string is_collapsed_string =
      visual_data.is_collapsed() ? "TRUE" : "FALSE";
  result += "\tis_collapsed: " + is_collapsed_string + "\n";
  result += "\trange_start: " + tab_range.ToString() + "\n}";

  return result;
}

}  // namespace app_restore