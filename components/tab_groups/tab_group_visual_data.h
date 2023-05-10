// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TAB_GROUPS_TAB_GROUP_VISUAL_DATA_H_
#define COMPONENTS_TAB_GROUPS_TAB_GROUP_VISUAL_DATA_H_

#include <stddef.h>

#include <string>

#include "base/component_export.h"
#include "components/tab_groups/tab_group_color.h"
#include "third_party/skia/include/core/SkColor.h"

namespace tab_groups {

class COMPONENT_EXPORT(TAB_GROUPS) TabGroupVisualData {
 public:
  // Construct a TabGroupVisualData with placeholder name and random color.
  TabGroupVisualData();
  TabGroupVisualData(std::u16string title,
                     tab_groups::TabGroupColorId color,
                     bool is_collapsed = false);
  TabGroupVisualData(std::u16string title,
                     uint32_t color_int,
                     bool is_collapsed = false);

  TabGroupVisualData(const TabGroupVisualData& other) = default;
  TabGroupVisualData(TabGroupVisualData&& other) = default;

  TabGroupVisualData& operator=(const TabGroupVisualData& other) = default;
  TabGroupVisualData& operator=(TabGroupVisualData&& other) = default;

  const std::u16string& title() const { return title_; }
  const tab_groups::TabGroupColorId& color() const { return color_; }
  bool is_collapsed() const { return is_collapsed_; }

  // Checks whether two instances are visually equivalent.
  bool operator==(const TabGroupVisualData& other) const {
    return title_ == other.title_ && color_ == other.color_;
  }
  bool operator!=(const TabGroupVisualData& other) const {
    return !(*this == other);
  }

  void SetTitle(const std::u16string& title) { title_ = title; }

 private:
  std::u16string title_;
  tab_groups::TabGroupColorId color_;
  bool is_collapsed_;
};

}  // namespace tab_groups

#endif  // COMPONENTS_TAB_GROUPS_TAB_GROUP_VISUAL_DATA_H_
