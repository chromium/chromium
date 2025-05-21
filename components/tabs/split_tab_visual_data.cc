// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/split_tab_visual_data.h"

namespace split_tabs {

SplitTabVisualData::SplitTabVisualData()
    : split_layout_(SplitTabLayout::kVertical) {}

SplitTabVisualData::SplitTabVisualData(SplitTabLayout split_layout)
    : split_layout_(split_layout) {}

SplitTabVisualData::SplitTabVisualData(SplitTabLayout split_layout,
                                       double split_ratio)
    : split_layout_(split_layout), split_ratio_(split_ratio) {}

SplitTabVisualData::~SplitTabVisualData() = default;

}  // namespace split_tabs
