// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAVED_TAB_GROUPS_EMPTY_TAB_GROUP_STORE_DELEGATE_H_
#define COMPONENTS_SAVED_TAB_GROUPS_EMPTY_TAB_GROUP_STORE_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "base/uuid.h"
#include "components/saved_tab_groups/tab_group_store_delegate.h"
#include "components/saved_tab_groups/tab_group_store_id.h"

namespace tab_groups {

// An empty implementation of the TabGroupStoreDelegate interface.
// The only functionality it has is to in fact invoke the `GetCallback` with an
// empty map when `GetAllTabGroupIDMetadatas(GetCallback)` is invoked. This is
// to ensure that initialization flows for the TabGroupStore still can work
// correctly.
class EmptyTabGroupStoreDelegate : public TabGroupStoreDelegate {
 public:
  EmptyTabGroupStoreDelegate();
  ~EmptyTabGroupStoreDelegate() override;

  // Disallow copy/assign.
  EmptyTabGroupStoreDelegate(const EmptyTabGroupStoreDelegate&) = delete;
  EmptyTabGroupStoreDelegate& operator=(const EmptyTabGroupStoreDelegate&) =
      delete;

  // TabGroupStoreDelegate implementation.
  void GetAllTabGroupIDMetadatas(GetCallback callback) override;
  void StoreTabGroupIDMetadata(
      const base::Uuid& sync_guid,
      const TabGroupIDMetadata& tab_group_id_metadata) override;
  void DeleteTabGroupIDMetdata(const base::Uuid& sync_guid) override;
};

}  // namespace tab_groups

#endif  // COMPONENTS_SAVED_TAB_GROUPS_EMPTY_TAB_GROUP_STORE_DELEGATE_H_
