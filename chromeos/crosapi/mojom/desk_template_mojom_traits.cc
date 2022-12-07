// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/mojom/desk_template_mojom_traits.h"

#include "base/strings/utf_string_conversions.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "ui/gfx/range/range.h"

namespace mojo {

std::string
StructTraits<crosapi::mojom::TabGroupDataView, tab_groups::TabGroupInfo>::title(
    const tab_groups::TabGroupInfo& group_info) {
  return base::UTF16ToUTF8(group_info.visual_data.title());
}

tab_groups::TabGroupColorId
StructTraits<crosapi::mojom::TabGroupDataView, tab_groups::TabGroupInfo>::color(
    const tab_groups::TabGroupInfo& group_info) {
  return group_info.visual_data.color();
}

bool StructTraits<crosapi::mojom::TabGroupDataView, tab_groups::TabGroupInfo>::
    is_collapsed(const tab_groups::TabGroupInfo& group_info) {
  return group_info.visual_data.is_collapsed();
}

int32_t
StructTraits<crosapi::mojom::TabGroupDataView, tab_groups::TabGroupInfo>::
    start_index(const tab_groups::TabGroupInfo& group_info) {
  return group_info.tab_range.start();
}

int32_t
StructTraits<crosapi::mojom::TabGroupDataView, tab_groups::TabGroupInfo>::
    tab_count(const tab_groups::TabGroupInfo& group_info) {
  return group_info.tab_range.length();
}

bool StructTraits<crosapi::mojom::TabGroupDataView, tab_groups::TabGroupInfo>::
    Read(crosapi::mojom::TabGroupDataView data,
         tab_groups::TabGroupInfo* out_group) {
  std::string data_title;
  if (!data.ReadTitle(&data_title))
    return false;

  tab_groups::TabGroupColorId color_id;
  if (!EnumTraits<tab_groups::mojom::Color,
                  tab_groups::TabGroupColorId>::FromMojom(data.color(),
                                                          &color_id)) {
    return false;
  }

  out_group->tab_range =
      gfx::Range(data.start_index(), data.start_index() + data.tab_count());
  out_group->visual_data = tab_groups::TabGroupVisualData(
      base::UTF8ToUTF16(data_title), color_id, data.is_collapsed());

  return true;
}

}  // namespace mojo