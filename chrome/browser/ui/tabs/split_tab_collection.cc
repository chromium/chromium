// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/split_tab_collection.h"

#include <memory>
#include <optional>

#include "chrome/browser/ui/tabs/tab_collection_storage.h"
#include "chrome/browser/ui/tabs/tab_model.h"

namespace tabs {

SplitTabCollection::SplitTabCollection(split_tabs::SplitTabId split_id)
    : TabCollection(TabCollection::Type::SPLIT, {}, true),
      split_id_(split_id) {}

SplitTabCollection::~SplitTabCollection() = default;

}  // namespace tabs
