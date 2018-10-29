// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_model_type_controller.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/sequence_checker.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/sync/base/cryptographer.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/engine/sync_encryption_handler.h"

namespace password_manager {

namespace {

// A SyncEncryptionHandler::Observer implementation that simply posts
// OnCryptographerStateChanged events to another task runner. This object is
// needed because its handed over to the sync thread, where the authoritative
// cryptographer lives. This class allows receiving cryptographer updates
// directly from the sync thread to the model thread (password's background
// thread).
class OnCryptographerStateChangedProxy
    : public syncer::SyncEncryptionHandler::Observer {
 public:
  // |cb| will be run on |task_runner|.
  OnCryptographerStateChangedProxy(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const base::RepeatingCallback<
          void(std::unique_ptr<syncer::Cryptographer>)>& cb)
      : task_runner_(std::move(task_runner)), cb_(cb) {
    DCHECK(task_runner_);
    DCHECK(cb_);
  }

  void OnPassphraseRequired(
      syncer::PassphraseRequiredReason reason,
      const syncer::KeyDerivationParams& key_derivation_params,
      const sync_pb::EncryptedData& pending_keys) override {}

  void OnPassphraseAccepted() override {}

  void OnBootstrapTokenUpdated(const std::string& bootstrap_token,
                               syncer::BootstrapTokenType type) override {}

  void OnEncryptedTypesChanged(syncer::ModelTypeSet encrypted_types,
                               bool encrypt_everything) override {}

  void OnEncryptionComplete() override {}

  void OnCryptographerStateChanged(
      syncer::Cryptographer* cryptographer) override {
    // We make a copy of |cryptographer| since it's not thread-safe.
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(cb_, std::make_unique<syncer::Cryptographer>(
                                           *cryptographer)));
  }

  void OnPassphraseTypeChanged(syncer::PassphraseType type,
                               base::Time passphrase_time) override {}

  void OnLocalSetPassphraseEncryption(
      const syncer::SyncEncryptionHandler::NigoriState& nigori_state) override {
  }

 private:
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const base::RepeatingCallback<void(std::unique_ptr<syncer::Cryptographer>)>
      cb_;

  DISALLOW_COPY_AND_ASSIGN(OnCryptographerStateChangedProxy);
};

}  // namespace

// Created and constructed on any thread, but otherwise used exclusively on a
// single sequence (the model sequence).
class PasswordModelTypeController::ModelCryptographerImpl
    : public syncer::SyncableServiceBasedBridge::ModelCryptographer {
 public:
  ModelCryptographerImpl() { DETACH_FROM_SEQUENCE(sequence_checker_); }

  void OnCryptographerStateChanged(
      std::unique_ptr<syncer::Cryptographer> cryptographer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(cryptographer);
    cryptographer_ = std::move(cryptographer);
  }

  base::Optional<syncer::ModelError> Decrypt(
      sync_pb::EntitySpecifics* specifics) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(cryptographer_);
    DCHECK(cryptographer_->is_ready());

    const sync_pb::PasswordSpecifics& password_specifics =
        specifics->password();
    DCHECK(password_specifics.has_encrypted());

    const sync_pb::EncryptedData& encrypted = password_specifics.encrypted();
    if (!cryptographer_->CanDecrypt(encrypted)) {
      return syncer::ModelError(FROM_HERE, "Cannot decrypt password");
    }

    sync_pb::EntitySpecifics unencrypted_password;
    if (!cryptographer_->Decrypt(encrypted,
                                 unencrypted_password.mutable_password()
                                     ->mutable_client_only_encrypted_data())) {
      return syncer::ModelError(FROM_HERE, "Failed to decrypt password");
    }

    unencrypted_password.Swap(specifics);
    return base::nullopt;
  }

  base::Optional<syncer::ModelError> Encrypt(
      sync_pb::EntitySpecifics* specifics) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(cryptographer_);
    DCHECK(cryptographer_->is_ready());

    const sync_pb::PasswordSpecificsData& data =
        specifics->password().client_only_encrypted_data();

    // We populate password metadata regardless of passphrase type, but it may
    // be cleared out later in NonBlockingTypeCommitContribution if an explicit
    // passphrase is used.
    sync_pb::EntitySpecifics encrypted_password;
    if (specifics->password().unencrypted_metadata().url() !=
        data.signon_realm()) {
      encrypted_password.mutable_password()
          ->mutable_unencrypted_metadata()
          ->set_url(data.signon_realm());
    }

    if (!cryptographer_->Encrypt(
            data, encrypted_password.mutable_password()->mutable_encrypted())) {
      return syncer::ModelError(FROM_HERE, "Failed to encrypt password");
    }

    encrypted_password.Swap(specifics);
    return base::nullopt;
  }

 private:
  ~ModelCryptographerImpl() override = default;

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<syncer::Cryptographer> cryptographer_;
};

PasswordModelTypeController::PasswordModelTypeController(
    syncer::OnceModelTypeStoreFactory store_factory,
    const base::RepeatingClosure& dump_stack,
    scoped_refptr<PasswordStore> password_store,
    syncer::SyncClient* sync_client)
    : PasswordModelTypeController(
          std::move(store_factory),
          dump_stack,
          std::move(password_store),
          sync_client,
          base::MakeRefCounted<ModelCryptographerImpl>()) {}

PasswordModelTypeController::~PasswordModelTypeController() = default;

void PasswordModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  sync_client_->GetSyncService()->AddObserver(this);
  NonUiSyncableServiceBasedModelTypeController::LoadModels(configure_context,
                                                           model_load_callback);
  sync_client_->GetPasswordStateChangedCallback().Run();
}

void PasswordModelTypeController::Stop(syncer::ShutdownReason shutdown_reason,
                                       StopCallback callback) {
  DCHECK(CalledOnValidThread());
  sync_client_->GetSyncService()->RemoveObserver(this);
  NonUiSyncableServiceBasedModelTypeController::Stop(shutdown_reason,
                                                     std::move(callback));
  sync_client_->GetPasswordStateChangedCallback().Run();
}

std::unique_ptr<syncer::SyncEncryptionHandler::Observer>
PasswordModelTypeController::GetEncryptionObserverProxy() {
  DCHECK(CalledOnValidThread());
  return std::make_unique<OnCryptographerStateChangedProxy>(
      background_task_runner_,
      base::BindRepeating(&ModelCryptographerImpl::OnCryptographerStateChanged,
                          model_cryptographer_));
}

void PasswordModelTypeController::OnStateChanged(syncer::SyncService* sync) {
  DCHECK(CalledOnValidThread());
  sync_client_->GetPasswordStateChangedCallback().Run();
}

PasswordModelTypeController::PasswordModelTypeController(
    syncer::OnceModelTypeStoreFactory store_factory,
    const base::RepeatingClosure& dump_stack,
    scoped_refptr<PasswordStore> password_store,
    syncer::SyncClient* sync_client,
    scoped_refptr<ModelCryptographerImpl> model_cryptographer)
    : NonUiSyncableServiceBasedModelTypeController(
          syncer::PASSWORDS,
          std::move(store_factory),
          base::BindRepeating(&PasswordStore::GetPasswordSyncableService,
                              password_store),
          dump_stack,
          password_store->GetBackgroundTaskRunner(),
          model_cryptographer),
      background_task_runner_(password_store->GetBackgroundTaskRunner()),
      model_cryptographer_(model_cryptographer),
      sync_client_(sync_client) {
  DCHECK(sync_client_);
  DCHECK(model_cryptographer_);
}

}  // namespace password_manager
