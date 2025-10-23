// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/tab_group_visual_data_traits.h"

#include "base/strings/utf_string_conversions.h"

std::string
mojo::StructTraits<MojoTabGroupVisualDataView, NativeTabGroupVisualData>::title(
    const NativeTabGroupVisualData& native) {
  return base::UTF16ToUTF8(native.title());
}

tab_groups::TabGroupColorId
mojo::StructTraits<MojoTabGroupVisualDataView, NativeTabGroupVisualData>::color(
    const NativeTabGroupVisualData& native) {
  return native.color();
}

bool mojo::StructTraits<MojoTabGroupVisualDataView, NativeTabGroupVisualData>::
    is_collapsed(const NativeTabGroupVisualData& native) {
  return native.is_collapsed();
}

bool mojo::StructTraits<MojoTabGroupVisualDataView, NativeTabGroupVisualData>::
    Read(MojoTabGroupVisualDataView data, NativeTabGroupVisualData* out) {
  std::string title;
  if (!data.ReadTitle(&title)) {
    return false;
  }

  tab_groups::TabGroupColorId color;
  if (!data.ReadColor(&color)) {
    return false;
  }

  *out = NativeTabGroupVisualData(base::UTF8ToUTF16(title), color,
                                  data.is_collapsed());
  return true;
}
