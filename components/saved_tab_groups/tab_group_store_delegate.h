// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_STORE_DELEGATE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_STORE_DELEGATE_H_

#include <map>

#include "base/functional/callback_forward.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/tab_group_store_id.h"

namespace tab_groups {

// A platform specific implementation for retrieving and storing sync GUID ->
// TabGroupIDMetadata mappings.
class TabGroupStoreDelegate {
 public:
  // A vector of sync GUID -> TabGroupIDMetadata mappings.
  using GetCallback =
      base::OnceCallback<void(std::map<base::Uuid, TabGroupIDMetadata>)>;

  TabGroupStoreDelegate() = default;
  virtual ~TabGroupStoreDelegate() = default;

  // Disallow copy/assign.
  TabGroupStoreDelegate(const TabGroupStoreDelegate&) = delete;
  TabGroupStoreDelegate& operator=(const TabGroupStoreDelegate&) = delete;

  // Retrieves all stored TabGroupIDMetadata.
  virtual void GetAllTabGroupIDMetadatas(GetCallback callback) = 0;

  // Maps the sync GUID to a TabGroupIDMetadata and store this in platform
  // specific storage. If `tab_group_id_metadata` is empty, deletes the entry.
  virtual void StoreTabGroupIDMetadata(
      const base::Uuid& sync_guid,
      const TabGroupIDMetadata& tab_group_id_metadata) = 0;

  // Delete metadata for a particular `sync_guid`.
  virtual void DeleteTabGroupIDMetdata(const base::Uuid& sync_guid) = 0;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_STORE_DELEGATE_H_
