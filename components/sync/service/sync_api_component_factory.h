// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_API_COMPONENT_FACTORY_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_API_COMPONENT_FACTORY_H_

#include <memory>
#include <string>

namespace signin {
class GaiaIdHash;
}

namespace syncer {

class SyncEngine;
class SyncInvalidationsService;

// This factory provides sync service code with the model type specific sync/api
// service (like SyncableService) implementations.
// TODO(crbug.com/335688372): Rename class to EngineLoader or similar to convey
// its scope.
class SyncApiComponentFactory {
 public:
  virtual ~SyncApiComponentFactory() = default;

  // Creating this in the factory helps us mock it out in testing.
  // `sync_invalidation_service` must not be null.
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

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_API_COMPONENT_FACTORY_H_
