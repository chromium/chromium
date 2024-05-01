// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_STORE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_STORE_H_

#include <map>
#include <optional>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/tab_group_store_delegate.h"
#include "components/saved_tab_groups/tab_group_store_id.h"

namespace tab_groups {

// Provides an in-memory cache and persistent storage for TabGroupIDMetadata.
class TabGroupStore {
 public:
  // Invoked when this has been initialized and is ready to serve requests.
  using InitCallback = base::OnceCallback<void()>;

  explicit TabGroupStore(std::unique_ptr<TabGroupStoreDelegate> delegate);
  virtual ~TabGroupStore();

  // Disallow copy/assign.
  TabGroupStore(const TabGroupStore&) = delete;
  TabGroupStore& operator=(const TabGroupStore&) = delete;

  // Uses the TabGroupStoreDelegate to fetch LocalIDs based on platform specific
  // storage. It is required to invoke this method before accessing the data.
  virtual void Initialize(InitCallback init_callback);

  // Returns all metadata known to the store.
  virtual std::map<base::Uuid, TabGroupIDMetadata> GetAllTabGroupIDMetadata()
      const;

  // Returns the latest cached version of the TabGroupIDMetadata for the
  // given sync GUID if it is known, else returns `std::nullopt`.
  virtual std::optional<TabGroupIDMetadata> GetTabGroupIDMetadata(
      const base::Uuid& sync_guid) const;

  // Stores a mapping from sync GUID to TabGroupIDMetadata. If `metadata` is
  // empty, deletes the entry.
  virtual void StoreTabGroupIDMetadata(const base::Uuid& sync_guid,
                                       const TabGroupIDMetadata& metadata);

  // Deletes metadata associated with the given `sync_guid`.
  virtual void DeleteTabGroupIDMetadata(const base::Uuid& sync_guid);

 private:
  // Used as a callback to results from the `TabStoreDelegate`.
  void OnGetTabGroupIdMetadatas(
      InitCallback init_callback,
      std::map<base::Uuid, TabGroupIDMetadata> mappings);

  // The platform specific delegate.
  std::unique_ptr<TabGroupStoreDelegate> delegate_;

  // In-memory cache of sync GUID -> TabGroupIDMetadata mapping.
  std::map<base::Uuid, TabGroupIDMetadata> cache_;

  base::WeakPtrFactory<TabGroupStore> weak_ptr_factory_{this};
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_TAB_GROUP_STORE_H_
