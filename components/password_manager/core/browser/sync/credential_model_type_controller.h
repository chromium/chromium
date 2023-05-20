// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_CREDENTIAL_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_CREDENTIAL_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/password_manager/core/browser/password_account_storage_settings_watcher.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/model_type_controller.h"
#include "components/sync/service/sync_service_observer.h"

class PrefService;

namespace syncer {
class ModelTypeControllerDelegate;
class SyncService;
}  // namespace syncer

namespace password_manager {

// A class that manages the startup and shutdown of password & passkey sync.
class CredentialModelTypeController : public syncer::ModelTypeController,
                                      public syncer::SyncServiceObserver,
                                      public signin::IdentityManager::Observer {
 public:
  CredentialModelTypeController(
      syncer::ModelType model_type,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_full_sync_mode,
      std::unique_ptr<syncer::ModelTypeControllerDelegate>
          delegate_for_transport_mode,
      PrefService* pref_service,
      signin::IdentityManager* identity_manager,
      syncer::SyncService* sync_service);

  CredentialModelTypeController(const CredentialModelTypeController&) = delete;
  CredentialModelTypeController& operator=(
      const CredentialModelTypeController&) = delete;

  ~CredentialModelTypeController() override;

  // DataTypeController overrides.
  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  void Stop(syncer::SyncStopMetadataFate fate, StopCallback callback) override;
  PreconditionState GetPreconditionState() const override;
  bool ShouldRunInTransportOnlyMode() const override;

  // SyncServiceObserver overrides.
  void OnStateChanged(syncer::SyncService* sync) override;

  // IdentityManager::Observer overrides.
  void OnAccountsInCookieUpdated(
      const signin::AccountsInCookieJarInfo& accounts_in_cookie_jar_info,
      const GoogleServiceAuthError& error) override;
  void OnAccountsCookieDeletedByUserAction() override;
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event) override;

 private:
  void OnOptInStateMaybeChanged();

  const raw_ptr<PrefService> pref_service_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  const raw_ptr<syncer::SyncService> sync_service_;

  PasswordAccountStorageSettingsWatcher account_storage_settings_watcher_;

  // Passed in to LoadModels(), and cached here for later use in Stop().
  syncer::SyncMode sync_mode_ = syncer::SyncMode::kFull;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};

  base::WeakPtrFactory<CredentialModelTypeController> weak_ptr_factory_{this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SYNC_CREDENTIAL_MODEL_TYPE_CONTROLLER_H_
