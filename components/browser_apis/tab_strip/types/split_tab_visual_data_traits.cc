// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/split_tab_visual_data_traits.h"

MojoSplitLayout mojo::EnumTraits<MojoSplitLayout, NativeSplitLayout>::ToMojom(
    NativeSplitLayout input) {
  switch (input) {
    case NativeSplitLayout::kVertical:
      return MojoSplitLayout::kVertical;
    case NativeSplitLayout::kHorizontal:
      return MojoSplitLayout::kHorizontal;
  }
  NOTREACHED();
}

bool mojo::EnumTraits<MojoSplitLayout, NativeSplitLayout>::FromMojom(
    MojoSplitLayout input,
    NativeSplitLayout* out) {
  switch (input) {
    case MojoSplitLayout::kVertical:
      *out = NativeSplitLayout::kVertical;
      return true;
    case MojoSplitLayout::kHorizontal:
      *out = NativeSplitLayout::kHorizontal;
      return true;
  }
  return false;
}

NativeSplitLayout mojo::StructTraits<
    MojoSplitTabVisualDataView,
    NativeSplitTabVisualData>::layout(const NativeSplitTabVisualData& native) {
  return native.split_layout();
}

double
mojo::StructTraits<MojoSplitTabVisualDataView, NativeSplitTabVisualData>::
    split_ratio(const NativeSplitTabVisualData& native) {
  return native.split_ratio();
}

bool mojo::StructTraits<MojoSplitTabVisualDataView, NativeSplitTabVisualData>::
    Read(MojoSplitTabVisualDataView view, NativeSplitTabVisualData* out) {
  NativeSplitLayout layout;
  if (!view.ReadLayout(&layout)) {
    return false;
  }

  *out = NativeSplitTabVisualData(layout, view.split_ratio());
  return true;
}
