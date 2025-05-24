// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TABS_PUBLIC_SPLIT_TAB_ID_H_
#define COMPONENTS_TABS_PUBLIC_SPLIT_TAB_ID_H_

#include <ostream>

#include "components/tab_groups/token_id.h"

namespace split_tabs {

class SplitTabId : public tab_groups::TokenId<SplitTabId> {};

// For use in std::unordered_map.
using SplitTabIdHash = tab_groups::TokenIdHash<SplitTabId>;

}  // namespace split_tabs

#endif  // COMPONENTS_TABS_PUBLIC_SPLIT_TAB_ID_H_
