// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/saved_tab_groups/empty_tab_group_store_delegate.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "components/saved_tab_groups/tab_group_store_delegate.h"

namespace tab_groups {

namespace {
void RunCallback(TabGroupStoreDelegate::GetCallback callback) {
  std::move(callback).Run({});
}
}  // namespace

EmptyTabGroupStoreDelegate::EmptyTabGroupStoreDelegate() = default;

EmptyTabGroupStoreDelegate::~EmptyTabGroupStoreDelegate() = default;

void EmptyTabGroupStoreDelegate::GetAllTabGroupIDMetadatas(
    TabGroupStoreDelegate::GetCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&RunCallback, std::move(callback)));
}

void EmptyTabGroupStoreDelegate::StoreTabGroupIDMetadata(
    const base::Uuid& sync_guid,
    const TabGroupIDMetadata& tab_group_id_metadata) {}

void EmptyTabGroupStoreDelegate::DeleteTabGroupIDMetdata(
    const base::Uuid& sync_guid) {}

}  // namespace tab_groups
