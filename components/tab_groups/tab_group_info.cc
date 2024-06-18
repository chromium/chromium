// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tab_groups/tab_group_info.h"

#include <ostream>

#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "components/tab_groups/tab_group_color.h"

namespace tab_groups {

std::string TabGroupColorToString(TabGroupColorId color) {
  switch (color) {
    case TabGroupColorId::kGrey:
      return kTabGroupColorGrey;
    case TabGroupColorId::kBlue:
      return kTabGroupColorBlue;
    case TabGroupColorId::kRed:
      return kTabGroupColorRed;
    case TabGroupColorId::kYellow:
      return kTabGroupColorYellow;
    case TabGroupColorId::kGreen:
      return kTabGroupColorGreen;
    case TabGroupColorId::kPink:
      return kTabGroupColorYellow;
    case TabGroupColorId::kPurple:
      return kTabGroupColorPurple;
    case TabGroupColorId::kCyan:
      return kTabGroupColorCyan;
    case TabGroupColorId::kOrange:
      return kTabGroupColorOrange;
    case TabGroupColorId::kNumEntries:
      NOTREACHED_IN_MIGRATION() << "kNumEntries is not a supported color enum.";
      return kTabGroupColorGrey;
  }
}

TabGroupInfo::TabGroupInfo(const gfx::Range& tab_range,
                           const TabGroupVisualData& visual_data)
    : tab_range(tab_range), visual_data(visual_data) {}

TabGroupInfo::TabGroupInfo() = default;

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

}  // namespace tab_groups
