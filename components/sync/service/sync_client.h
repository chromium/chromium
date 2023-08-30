// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_SYNC_CLIENT_H_
#define COMPONENTS_SYNC_SERVICE_SYNC_CLIENT_H_

#include <map>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "components/sync/base/extensions_activity.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/data_type_controller.h"

class PrefService;

namespace signin {
class IdentityManager;
}

namespace trusted_vault {
class TrustedVaultClient;
}

namespace syncer {

struct LocalDataDescription;
class SyncApiComponentFactory;
class SyncInvalidationsService;
class SyncService;
class SyncTypePreferenceProvider;

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

  virtual SyncInvalidationsService* GetSyncInvalidationsService() = 0;
  virtual trusted_vault::TrustedVaultClient* GetTrustedVaultClient() = 0;
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

  // Queries the count and description/preview of existing local data for
  // `types` data types. This is an asynchronous method which returns the result
  // via the callback `callback` once the information for all the data types in
  // `types` is available.
  // Note: Only data types that are enabled and support this functionality are
  // part of the response.
  // TODO(crbug.com/1451508): Mark as pure virtual once all implementations have
  // overridden this.
  virtual void GetLocalDataDescriptions(
      ModelTypeSet types,
      base::OnceCallback<void(std::map<ModelType, LocalDataDescription>)>
          callback);

  // Requests the client to move all local data to account for `types` data
  // types. This is an asynchronous method which moves the local data for all
  // `types` to the account store locally. Upload to the server will happen as
  // part of the regular commit process, and is NOT part of this method.
  // TODO(crbug.com/1451508): Mark as pure virtual once all implementations have
  // overridden this.
  virtual void TriggerLocalDataMigration(ModelTypeSet types);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_SYNC_CLIENT_H_
