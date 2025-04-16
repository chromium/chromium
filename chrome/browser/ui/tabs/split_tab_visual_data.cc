// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_visual_data.h"

namespace split_tabs {

SplitTabVisualData::SplitTabVisualData()
    : split_layout_(SplitTabLayout::kHorizontal), split_ratio_(0.5) {}

SplitTabVisualData::SplitTabVisualData(SplitTabLayout split_layout,
                                       double split_ratio)
    : split_layout_(split_layout), split_ratio_(split_ratio) {}

SplitTabVisualData::~SplitTabVisualData() = default;

}  // namespace split_tabs
