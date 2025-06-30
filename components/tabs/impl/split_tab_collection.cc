// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/split_tab_collection.h"

#include <memory>
#include <optional>

#include "components/tabs/public/split_tab_data.h"
#include "components/tabs/public/split_tab_visual_data.h"
#include "components/tabs/public/tab_collection_storage.h"

namespace tabs {

SplitTabCollection::SplitTabCollection(
    split_tabs::SplitTabId split_id,
    split_tabs::SplitTabVisualData visual_data)
    : TabCollection(TabCollection::Type::SPLIT, {}, true),
      split_tab_data_(std::make_unique<split_tabs::SplitTabData>(this,
                                                                 split_id,
                                                                 visual_data)) {
}

SplitTabCollection::~SplitTabCollection() = default;

split_tabs::SplitTabId SplitTabCollection::GetSplitTabId() const {
  return split_tab_data_->id();
}

}  // namespace tabs
