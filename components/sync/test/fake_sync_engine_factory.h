// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TEST_FAKE_SYNC_ENGINE_FACTORY_H_
#define COMPONENTS_SYNC_TEST_FAKE_SYNC_ENGINE_FACTORY_H_

#include <memory>
#include <string>

#include "components/sync/base/data_type.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/service/sync_engine_factory.h"

namespace syncer {

class FakeSyncEngine;

class FakeSyncEngineFactory : public SyncEngineFactory {
 public:
  FakeSyncEngineFactory();
  ~FakeSyncEngineFactory() override;

  // Enables or disables FakeSyncEngine's synchronous completion of
  // Initialize(). Defaults to true.
  void AllowFakeEngineInitCompletion(bool allow);

  FakeSyncEngine* last_created_engine() { return last_created_engine_.get(); }

  // Determines whether future initialization of FakeSyncEngine will report
  // being an initial sync.
  void set_first_time_sync_configure_done(bool done) {
    is_first_time_sync_configure_done_ = done;
  }

  // SyncEngineFactory overrides.
  std::unique_ptr<SyncEngine> CreateSyncEngine(
      const std::string& name,
      const signin::GaiaIdHash& gaia_id_hash,
      syncer::SyncInvalidationsService* sync_invalidations_service) override;
  bool HasTransportDataIncludingFirstSync(
      const signin::GaiaIdHash& gaia_id_hash) override;
  void CleanupOnDisableSync() override;
  void ClearTransportDataForAccount(
      const signin::GaiaIdHash& gaia_id_hash) override;

 private:
  base::WeakPtr<FakeSyncEngine> last_created_engine_;
  bool allow_fake_engine_init_completion_ = true;
  bool is_first_time_sync_configure_done_ = false;
  base::WeakPtrFactory<FakeSyncEngineFactory> weak_factory_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TEST_FAKE_SYNC_ENGINE_FACTORY_H_
