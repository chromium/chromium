// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tabs/public/pinned_tab_collection.h"

namespace tabs {

PinnedTabCollection::PinnedTabCollection()
    : TabCollection(TabCollection::Type::PINNED,
                    {TabCollection::Type::SPLIT},
                    /*supports_tabs=*/true) {}

PinnedTabCollection::~PinnedTabCollection() = default;

}  // namespace tabs
