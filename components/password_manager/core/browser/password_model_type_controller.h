// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/sync/driver/non_ui_syncable_service_based_model_type_controller.h"
#include "components/sync/driver/sync_service_observer.h"

namespace syncer {
class SyncClient;
}  // namespace syncer

namespace password_manager {

class PasswordStore;

// A class that manages the startup and shutdown of password sync.
class PasswordModelTypeController
    : public syncer::NonUiSyncableServiceBasedModelTypeController,
      public syncer::SyncServiceObserver {
 public:
  // |password_store| must not be null and is used to persist the encrypted
  // copy of sync's data and metadata, sync-ed with |password_store|.
  // |dump_stack| is called when a unrecoverable error occurs. |sync_client|
  // must not be null.
  PasswordModelTypeController(syncer::OnceModelTypeStoreFactory store_factory,
                              const base::RepeatingClosure& dump_stack,
                              scoped_refptr<PasswordStore> password_store,
                              syncer::SyncClient* sync_client);
  ~PasswordModelTypeController() override;

  // DataTypeController overrides.
  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;
  void Stop(syncer::ShutdownReason shutdown_reason,
            StopCallback callback) override;
  std::unique_ptr<syncer::SyncEncryptionHandler::Observer>
  GetEncryptionObserverProxy() override;

  // SyncServiceObserver overrides.
  void OnStateChanged(syncer::SyncService* sync) override;

 private:
  class ModelCryptographerImpl;

  // Constructor overload to make sure |ModelCryptographerImpl| gets constructed
  // before the base class.
  PasswordModelTypeController(
      syncer::OnceModelTypeStoreFactory store_factory,
      const base::RepeatingClosure& dump_stack,
      scoped_refptr<PasswordStore> password_store,
      syncer::SyncClient* sync_client,
      scoped_refptr<ModelCryptographerImpl> model_cryptographer);

  const scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  const scoped_refptr<ModelCryptographerImpl> model_cryptographer_;
  syncer::SyncClient* const sync_client_;

  DISALLOW_COPY_AND_ASSIGN(PasswordModelTypeController);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MODEL_TYPE_CONTROLLER_H_
