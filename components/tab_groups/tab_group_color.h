// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TAB_GROUPS_TAB_GROUP_COLOR_H_
#define COMPONENTS_TAB_GROUPS_TAB_GROUP_COLOR_H_

#include <stddef.h>
#include <map>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "third_party/skia/include/core/SkColor.h"

namespace tab_groups {

// IMPORTANT: Do not change or reuse the values of any item in this enum.
// These values are written to and read from disk for session and tab restore.
//
// Any changes to the tab group color set should be made in the map returned by
// GetTabGroupColorLabelMap(). The set of valid colors is contained in the keys
// of that map. Do not add or delete items in this enum without also reflecting
// that change in the map.
//
// Any code that reads an enum value from disk should check it against the map
// from GetTabGroupColorLabelMap(). If the value is not contained in the map's
// keys, default to kGrey.
//
// Additionally, any colors added here will also be used in
// chrome/browser/resources/tab_search/tab_group_color_helper.ts. As such these
// colors should be kept in sync. Ex: Adding orange in this file,
// requires adding orange in the other file.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.tab_groups
//
enum class TabGroupColorId {
  kGrey = 0,
  kBlue = 1,
  kRed = 2,
  kYellow = 3,
  kGreen = 4,
  kPink = 5,
  kPurple = 6,
  kCyan = 7,
  kOrange = 8,
  // Next value: 9
  kNumEntries = 9,
};

using ColorLabelMap = base::flat_map<TabGroupColorId, std::u16string>;

// Returns a map of TabGroupColorIds to their string labels.
// When reading color IDs from disk, always verify against the keys in this
// map for valid values.
COMPONENT_EXPORT(TAB_GROUPS)
const ColorLabelMap& GetTabGroupColorLabelMap();

// Returns the least-used color in the color set, breaking ties toward the first
// color in the set. Used to initialize a new group's color, which should be as
// distinct from the other groups as possible.
COMPONENT_EXPORT(TAB_GROUPS)
TabGroupColorId GetNextColor(const std::vector<TabGroupColorId>& used_colors);

}  // namespace tab_groups

#endif  // COMPONENTS_TAB_GROUPS_TAB_GROUP_COLOR_H_
