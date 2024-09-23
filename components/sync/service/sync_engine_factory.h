// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_ENGINE_FACTORY_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_ENGINE_FACTORY_H_

#include <memory>
#include <string>

namespace signin {
class GaiaIdHash;
}  // namespace signin

namespace syncer {

class SyncEngine;
class SyncInvalidationsService;

// Class responsible for instantiating SyncEngine and restoring its state
// (transport data including cache GUID, birthday, Nigori, etc.). In addition to
// acting as a factory, it fully abstrcts and encapsulates the storage of
// transport data and offers APIs to interact with this data even without having
// to instantiate SyncEngine.
class SyncEngineFactory {
 public:
  virtual ~SyncEngineFactory() = default;

  // Instantiates SyncEngine for a specific account determined by
  // `gaia_id_hash`. `sync_invalidation_service` must not be null. `name` is for
  // logging purposes only, useful in integration tests that involve multiple
  // clients.
  virtual std::unique_ptr<SyncEngine> CreateSyncEngine(
      const std::string& name,
      const signin::GaiaIdHash& gaia_id_hash,
      SyncInvalidationsService* sync_invalidation_service) = 0;

  // Returns whether the local transport data indicates that a sync engine
  // previously initialized successfully and hence populated at least some
  // transport data (e.g. birthday). It also implies that the client
  // successfully communicated to the server at least once.
  virtual bool HasTransportDataIncludingFirstSync(
      const signin::GaiaIdHash& gaia_id_hash) = 0;

  // Cleans up potentially-leftover sync data. Usually the SyncEngine is
  // responsible for that; this is meant to be called if sync gets disabled
  // while the engine doesn't exist.
  virtual void CleanupOnDisableSync() = 0;

  // Clears local transport data (cache GUID etc) for the given account.
  virtual void ClearTransportDataForAccount(
      const signin::GaiaIdHash& gaia_id_hash) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_ENGINE_FACTORY_H_
