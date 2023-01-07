// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tab_groups/tab_group_visual_data.h"

#include <string>

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "components/tab_groups/tab_group_color.h"

namespace tab_groups {

TabGroupVisualData::TabGroupVisualData()
    : TabGroupVisualData(std::u16string(), TabGroupColorId::kGrey, false) {}

TabGroupVisualData::TabGroupVisualData(std::u16string title,
                                       tab_groups::TabGroupColorId color,
                                       bool is_collapsed)
    : title_(std::move(title)), color_(color), is_collapsed_(is_collapsed) {}

TabGroupVisualData::TabGroupVisualData(std::u16string title,
                                       uint32_t color_int,
                                       bool is_collapsed)
    : title_(std::move(title)),
      color_(TabGroupColorId::kGrey),
      is_collapsed_(is_collapsed) {
  auto color_id = static_cast<tab_groups::TabGroupColorId>(color_int);
  if (base::Contains(tab_groups::GetTabGroupColorLabelMap(), color_id))
    color_ = color_id;
}

}  // namespace tab_groups
