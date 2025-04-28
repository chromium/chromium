// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_DATA_TYPE_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_DATA_TYPE_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/sync_mode.h"
#include "components/sync/service/data_type_controller.h"
#include "components/sync/service/data_type_local_data_batch_uploader.h"

class PrefService;

namespace syncer {
class DataTypeControllerDelegate;
}  // namespace syncer

namespace password_manager {

// A class that manages the startup and shutdown of password sync.
class PasswordDataTypeController : public syncer::DataTypeController,
                                    public signin::IdentityManager::Observer {
 public:
  // Note: Android might always be configured in transport mode if
  // UnifiedPasswordManagerLocalPasswordsAndroid* flags are in place.
  PasswordDataTypeController(
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::DataTypeControllerDelegate>
          delegate_for_transport_mode,
      std::unique_ptr<syncer::DataTypeLocalDataBatchUploader> batch_uploader,
      PrefService* pref_service,
      signin::IdentityManager* identity_manager);

  PasswordDataTypeController(const PasswordDataTypeController&) = delete;
  PasswordDataTypeController& operator=(const PasswordDataTypeController&) =
      delete;

  ~PasswordDataTypeController() override;

  // DataTypeController overrides.
  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  void Stop(syncer::SyncStopMetadataFate fate, StopCallback callback) override;

  // IdentityManager::Observer overrides.
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;
  void OnIdentityManagerShutdown(
      signin::IdentityManager* identity_manager) override;

 private:
  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  syncer::SyncMode sync_mode_ = syncer::SyncMode::kFull;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_PASSWORD_DATA_TYPE_CONTROLLER_H_
