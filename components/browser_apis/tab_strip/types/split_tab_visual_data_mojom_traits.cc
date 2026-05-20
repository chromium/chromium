// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_apis/tab_strip/types/split_tab_visual_data_mojom_traits.h"

MojoSplitLayout mojo::EnumTraits<MojoSplitLayout, NativeSplitLayout>::ToMojom(
    NativeSplitLayout input) {
  switch (input) {
    case NativeSplitLayout::kSideBySide:
      return MojoSplitLayout::kSideBySide;
    case NativeSplitLayout::kStacked:
      return MojoSplitLayout::kStacked;
  }
  NOTREACHED();
}

NativeSplitLayout
mojo::EnumTraits<MojoSplitLayout, NativeSplitLayout>::FromMojom(
    MojoSplitLayout input) {
  switch (input) {
    case MojoSplitLayout::kSideBySide:
      return NativeSplitLayout::kSideBySide;
    case MojoSplitLayout::kStacked:
      return NativeSplitLayout::kStacked;
  }
  NOTREACHED();
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
