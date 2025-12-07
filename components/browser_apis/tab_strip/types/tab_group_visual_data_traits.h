// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_TAB_GROUP_VISUAL_DATA_TRAITS_H_
#define COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_TAB_GROUP_VISUAL_DATA_TRAITS_H_

#include "base/strings/utf_string_conversions.h"
#include "components/browser_apis/tab_strip/tab_strip_api_data_model.mojom.h"
#include "components/tab_groups/public/mojom/tab_groups_mojom_traits.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

using MojoTabGroupVisualDataView = tabs_api::mojom::TabGroupVisualDataDataView;
using NativeTabGroupVisualData = tab_groups::TabGroupVisualData;

template <>
struct mojo::StructTraits<MojoTabGroupVisualDataView,
                          NativeTabGroupVisualData> {
  // Field getters:
  static std::string title(const NativeTabGroupVisualData& native);
  static tab_groups::TabGroupColorId color(
      const NativeTabGroupVisualData& native);
  static bool is_collapsed(const NativeTabGroupVisualData& native);

  // Decoder:
  static bool Read(MojoTabGroupVisualDataView view,
                   NativeTabGroupVisualData* out);
};

#endif  // COMPONENTS_BROWSER_APIS_TAB_STRIP_TYPES_TAB_GROUP_VISUAL_DATA_TRAITS_H_
