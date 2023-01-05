// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_CLIENT_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_CLIENT_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_controller.h"

class PrefService;

namespace invalidation {
class InvalidationService;
}  // namespace invalidation

namespace signin {
class IdentityManager;
}

namespace syncer {

class SyncApiComponentFactory;
class SyncInvalidationsService;
class SyncService;
class SyncTypePreferenceProvider;
class TrustedVaultClient;

// Interface for clients of the Sync API to plumb through necessary dependent
// components. This interface is purely for abstracting dependencies, and
// should not contain any non-trivial functional logic.
//
// Note: on some platforms, getters might return nullptr. Callers are expected
// to handle these scenarios gracefully.
class SyncClient {
 public:
  SyncClient() = default;
  SyncClient(const SyncClient&) = delete;
  SyncClient& operator=(const SyncClient&) = delete;
  virtual ~SyncClient() = default;

  // Returns the current profile's preference service.
  virtual PrefService* GetPrefService() = 0;

  virtual signin::IdentityManager* GetIdentityManager() = 0;

  // Returns the path to the folder used for storing the local sync database.
  // It is only used when sync is running against a local backend.
  virtual base::FilePath GetLocalSyncBackendFolder() = 0;

  // Returns a vector with all supported datatypes and their controllers.
  virtual DataTypeController::TypeVector CreateDataTypeControllers(
      SyncService* sync_service) = 0;

  virtual invalidation::InvalidationService* GetInvalidationService() = 0;
  virtual SyncInvalidationsService* GetSyncInvalidationsService() = 0;
  virtual TrustedVaultClient* GetTrustedVaultClient() = 0;
  virtual scoped_refptr<ExtensionsActivity> GetExtensionsActivity() = 0;

  // Returns the current SyncApiComponentFactory instance.
  virtual SyncApiComponentFactory* GetSyncApiComponentFactory() = 0;

  // Returns the preference provider, or null if none exists.
  virtual SyncTypePreferenceProvider* GetPreferenceProvider() = 0;

  // Notifies the client that local sync metadata in preferences has been
  // cleared.
  // TODO(crbug.com/1137346): Replace this mechanism with a more universal one,
  // e.g. using SyncServiceObserver.
  virtual void OnLocalSyncTransportDataCleared() = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_CLIENT_H_
