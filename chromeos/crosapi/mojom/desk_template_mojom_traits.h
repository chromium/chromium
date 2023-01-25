// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_MOJOM_DESK_TEMPLATE_MOJOM_TRAITS_H_
#define CHROMEOS_CROSAPI_MOJOM_DESK_TEMPLATE_MOJOM_TRAITS_H_

#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_info.h"

#include "chromeos/crosapi/mojom/desk_template.mojom.h"

namespace mojo {

template <>
struct StructTraits<crosapi::mojom::TabGroupDataView,
                    tab_groups::TabGroupInfo> {
  static std::string title(const tab_groups::TabGroupInfo& group_info);
  static tab_groups::TabGroupColorId color(
      const tab_groups::TabGroupInfo& group_info);
  static bool is_collapsed(const tab_groups::TabGroupInfo& group_info);
  static int32_t start_index(const tab_groups::TabGroupInfo& group_info);
  static int32_t tab_count(const tab_groups::TabGroupInfo& tab_count);

  static bool Read(crosapi::mojom::TabGroupDataView data,
                   tab_groups::TabGroupInfo* out_group);
};

}  // namespace mojo

#endif  // CHROMEOS_CROSAPI_MOJOM_DESK_TEMPLATE_MOJOM_TRAITS_H_
