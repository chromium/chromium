// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/tab_group_store.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/saved_tab_groups/tab_group_store_delegate.h"

namespace tab_groups {

TabGroupStore::TabGroupStore(std::unique_ptr<TabGroupStoreDelegate> delegate)
    : delegate_(std::move(delegate)) {
  CHECK(delegate_);
}

TabGroupStore::~TabGroupStore() = default;

void TabGroupStore::Initialize(InitCallback init_callback) {
  delegate_->GetAllTabGroupIDMetadatas(
      base::BindOnce(&TabGroupStore::OnGetTabGroupIdMetadatas,
                     weak_ptr_factory_.GetWeakPtr(), std::move(init_callback)));
}

std::map<base::Uuid, TabGroupIDMetadata>
TabGroupStore::GetAllTabGroupIDMetadata() const {
  return cache_;
}

std::optional<TabGroupIDMetadata> TabGroupStore::GetTabGroupIDMetadata(
    const base::Uuid& sync_guid) const {
  auto it = cache_.find(sync_guid);
  if (it == cache_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void TabGroupStore::StoreTabGroupIDMetadata(
    const base::Uuid& sync_guid,
    const TabGroupIDMetadata& metadata) {
  cache_.emplace(sync_guid, metadata);
  delegate_->StoreTabGroupIDMetadata(sync_guid, std::move(metadata));
}

void TabGroupStore::DeleteTabGroupIDMetadata(const base::Uuid& sync_guid) {
  cache_.erase(sync_guid);
  delegate_->DeleteTabGroupIDMetdata(sync_guid);
}

void TabGroupStore::OnGetTabGroupIdMetadatas(
    InitCallback init_callback,
    std::map<base::Uuid, TabGroupIDMetadata> mappings) {
  for (const auto& mapping : mappings) {
    cache_.emplace(mapping);
  }
  std::move(init_callback).Run();
}

}  // namespace tab_groups
