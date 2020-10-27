// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/local_search_service_sync_proxy.h"

#include "chromeos/components/local_search_service/index_sync_proxy.h"
#include "chromeos/components/local_search_service/local_search_service_sync.h"
#include "chromeos/components/local_search_service/shared_structs.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace local_search_service {

LocalSearchServiceSyncProxy::LocalSearchServiceSyncProxy(
    local_search_service::LocalSearchServiceSync* local_search_service)
    : service_(local_search_service) {
  DCHECK(service_);
}

LocalSearchServiceSyncProxy::~LocalSearchServiceSyncProxy() = default;

void LocalSearchServiceSyncProxy::GetIndex(
    IndexId index_id,
    Backend backend,
    mojo::PendingReceiver<mojom::IndexSyncProxy> index_receiver) {
  GetIndex(index_id, backend, nullptr, std::move(index_receiver));
}

void LocalSearchServiceSyncProxy::GetIndex(
    IndexId index_id,
    Backend backend,
    PrefService* local_state,
    mojo::PendingReceiver<mojom::IndexSyncProxy> index_receiver) {
  auto it = indices_.find(index_id);
  if (it == indices_.end()) {
    IndexSync* index = service_->GetIndexSync(index_id, backend, local_state);
    it = indices_.emplace(index_id, std::make_unique<IndexSyncProxy>(index))
             .first;
  }
  it->second->BindReceiver(std::move(index_receiver));
}

void LocalSearchServiceSyncProxy::BindReceiver(
    mojo::PendingReceiver<mojom::LocalSearchServiceSyncProxy> receiver) {
  receivers_.Add(this, std::move(receiver));
}

}  // namespace local_search_service
}  // namespace chromeos
