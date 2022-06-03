// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_TAB_GROUP_INFO_H_
#define COMPONENTS_APP_RESTORE_TAB_GROUP_INFO_H_

#include "base/component_export.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/gfx/range/range.h"

namespace app_restore {

// String kConstants used by TabGroupColorToString.
constexpr char kTabGroupColorUnknown[] = "UNKNONW";
constexpr char kTabGroupColorGrey[] = "GREY";
constexpr char kTabGroupColorBlue[] = "BLUE";
constexpr char kTabGroupColorRed[] = "RED";
constexpr char kTabGroupColorYellow[] = "YELLOW";
constexpr char kTabGroupColorGreen[] = "GREEN";
constexpr char kTabGroupColorPink[] = "PINK";
constexpr char kTabGroupColorPurple[] = "PURPLE";
constexpr char kTabGroupColorCyan[] = "CYAN";
constexpr char kTabGroupColorOrange[] = "ORANGE";

// Used in ToString as well as in Conversion Logic for
// components/desks_storage/core/desk_template_conversion.cc
std::string COMPONENT_EXPORT(APP_RESTORE)
    TabGroupColorToString(tab_groups::TabGroupColorId color);

// Tab group info is a structure representing a tab group that
// is associated with a specific browser window.  This struct lives
// in a list of instances of its kind located under the tab_group_infos
// field of an AppRestoreData struct.  This structure is used by saved desks
// to store data relating to tab groups and is not directly used by full
// restore.
struct COMPONENT_EXPORT(APP_RESTORE) TabGroupInfo {
  TabGroupInfo(const gfx::Range& tab_range,
               const tab_groups::TabGroupVisualData& visual_data);

  TabGroupInfo(const TabGroupInfo&);
  TabGroupInfo& operator=(const TabGroupInfo& other);

  // Move constructor used for vector allocation.
  TabGroupInfo(TabGroupInfo&& other);

  ~TabGroupInfo();

  // Checks whether or not two TabGroupInfos are semantically equivalent.
  // Used in testing.
  bool operator==(const TabGroupInfo& other) const;

  // Produces a string representation of this tab group used in debugging.
  std::string ToString() const;

  // Range of tabs this group is associated with.
  gfx::Range tab_range;

  // Human readable data associated with this tab group.
  tab_groups::TabGroupVisualData visual_data;
};

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_TAB_GROUP_INFO_H_