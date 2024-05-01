// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_store_id.h"

#include "components/saved_tab_groups/types.h"

namespace tab_groups {

TabGroupIDMetadata::TabGroupIDMetadata(LocalTabGroupID tab_group_id)
    : local_tab_group_id(tab_group_id) {}

bool TabGroupIDMetadata::operator==(const TabGroupIDMetadata& other) const {
  return local_tab_group_id == other.local_tab_group_id;
}

}  // namespace tab_groups
