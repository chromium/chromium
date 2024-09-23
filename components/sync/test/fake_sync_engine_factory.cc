// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_engine_factory.h"

#include <utility>

#include "base/test/bind.h"
#include "components/sync/test/fake_sync_engine.h"

namespace syncer {

FakeSyncEngineFactory::FakeSyncEngineFactory() = default;

FakeSyncEngineFactory::~FakeSyncEngineFactory() = default;

void FakeSyncEngineFactory::AllowFakeEngineInitCompletion(bool allow) {
  allow_fake_engine_init_completion_ = allow;
}

std::unique_ptr<SyncEngine> FakeSyncEngineFactory::CreateSyncEngine(
    const std::string& name,
    const signin::GaiaIdHash& gaia_id_hash,
    syncer::SyncInvalidationsService* sync_invalidations_service) {
  auto engine = std::make_unique<FakeSyncEngine>(
      allow_fake_engine_init_completion_,
      /*is_first_time_sync_configure=*/!is_first_time_sync_configure_done_,
      /*sync_transport_data_cleared_cb=*/
      base::BindRepeating(&FakeSyncEngineFactory::CleanupOnDisableSync,
                          weak_factory_.GetWeakPtr()));
  last_created_engine_ = engine->AsWeakPtr();
  return engine;
}

bool FakeSyncEngineFactory::HasTransportDataIncludingFirstSync(
    const signin::GaiaIdHash& gaia_id_hash) {
  return is_first_time_sync_configure_done_;
}

void FakeSyncEngineFactory::CleanupOnDisableSync() {
  is_first_time_sync_configure_done_ = false;
}

void FakeSyncEngineFactory::ClearTransportDataForAccount(
    const signin::GaiaIdHash& gaia_id_hash) {}

}  // namespace syncer
