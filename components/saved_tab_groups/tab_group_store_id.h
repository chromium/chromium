// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_STORE_ID_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_STORE_ID_H_

#include <stdint.h>

#include "components/saved_tab_groups/types.h"

namespace tab_groups {

// A generic representation of Tab Group ID metadata.
struct TabGroupIDMetadata {
 public:
  explicit TabGroupIDMetadata(LocalTabGroupID tab_group_id);

  // A local and platform specific ID for the tab group.
  LocalTabGroupID local_tab_group_id;

  bool operator==(const TabGroupIDMetadata& other) const;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_STORE_ID_H_
