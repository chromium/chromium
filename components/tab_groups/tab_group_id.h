// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TAB_GROUPS_TAB_GROUP_ID_H_
#define COMPONENTS_TAB_GROUPS_TAB_GROUP_ID_H_

#include <ostream>

#include "base/component_export.h"
#include "components/tab_groups/token_id.h"

namespace tab_groups {

class COMPONENT_EXPORT(TAB_GROUPS) TabGroupId
    : public tab_groups::TokenId<TabGroupId> {};

// For use in std::unordered_map.
using TabGroupIdHash = tab_groups::TokenIdHash<TabGroupId>;

}  // namespace tab_groups

#endif  // COMPONENTS_TAB_GROUPS_TAB_GROUP_ID_H_
