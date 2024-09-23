// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/pending_local_nigori_commit.h"

#include <utility>

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_key.h"
#include "components/sync/engine/nigori/cross_user_sharing_public_private_key_pair.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/engine/nigori/nigori.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/keystore_keys_cryptographer.h"
#include "components/sync/nigori/nigori_state.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

namespace {

using sync_pb::NigoriSpecifics;

// Populates a new key pair with a version 0 to `state`, or replaces the
// existing key pair with the current key version. This method does not affect
// key pairs with other versions.
void InitNewOrFixCorruptedKeyPair(
    const CrossUserSharingPublicPrivateKeyPair& cross_user_sharing_key_pair,
    NigoriState* state) {
  CHECK(state->NeedsGenerateCrossUserSharingKeyPair());

  // Keep the existing version in case if the key pair is corrupted.
  const uint32_t version =
      state->cross_user_sharing_key_pair_version.value_or(0);

  state->cross_user_sharing_public_key =
      CrossUserSharingPublicKey::CreateByImport(
          cross_user_sharing_key_pair.GetRawPublicKey());
  state->cross_user_sharing_key_pair_version = version;
  std::optional<CrossUserSharingPublicPrivateKeyPair> key_pair =
      CrossUserSharingPublicPrivateKeyPair::CreateByImport(
          cross_user_sharing_key_pair.GetRawPrivateKey());
  CHECK(key_pair.has_value());
  state->cryptographer->SetKeyPair(std::move(key_pair.value()), version);
  state->cryptographer->SelectDefaultCrossUserSharingKey(version);
}

void LogCrossUserSharingPublicPrivateKeyInit(bool is_succesful) {
  base::UmaHistogramBoolean("Sync.CrossUserSharingPublicPrivateKeyInitSuccess",
                            is_succesful);
}

class CustomPassphraseSetter : public PendingLocalNigoriCommit {
 public:
  explicit CustomPassphraseSetter(
      const std::string& passphrase,
      const KeyDerivationParams& key_derivation_params)
      : passphrase_(passphrase),
        key_derivation_params_(key_derivation_params) {}

  CustomPassphraseSetter(const CustomPassphraseSetter&) = delete;
  CustomPassphraseSetter& operator=(const CustomPassphraseSetter&) = delete;

  ~CustomPassphraseSetter() override = default;

  bool TryApply(NigoriState* state) const override {
    if (state->pending_keys.has_value()) {
      return false;
    }

    switch (state->passphrase_type) {
      case NigoriSpecifics::UNKNOWN:
        return false;
      case NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE:
      case NigoriSpecifics::CUSTOM_PASSPHRASE:
        // Attempt to set the explicit passphrase when one was already set. It's
        // possible if we received new NigoriSpecifics during the passphrase
        // setup.
        DVLOG(1)
            << "Attempt to set explicit passphrase failed, because one was "
               "already set.";
        return false;
      case NigoriSpecifics::IMPLICIT_PASSPHRASE:
      case NigoriSpecifics::KEYSTORE_PASSPHRASE:
      case NigoriSpecifics::TRUSTED_VAULT_PASSPHRASE:
        break;
    }

    const std::string default_key_name =
        state->cryptographer->EmplaceKey(passphrase_, key_derivation_params_);
    if (default_key_name.empty()) {
      DLOG(ERROR) << "Failed to set encryption passphrase";
      return false;
    }

    state->cryptographer->SelectDefaultEncryptionKey(default_key_name);
    state->pending_keystore_decryptor_token.reset();
    state->passphrase_type = NigoriSpecifics::CUSTOM_PASSPHRASE;
    state->custom_passphrase_key_derivation_params = key_derivation_params_;
    state->encrypt_everything = true;
    state->custom_passphrase_time = base::Time::Now();

    return true;
  }

  void OnSuccess(const NigoriState& state,
                 SyncEncryptionHandler::Observer* observer) override {
    DCHECK(!state.pending_keys.has_value());

    observer->OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                      state.custom_passphrase_time);
    observer->OnCryptographerStateChanged(state.cryptographer.get(),
                                          /*has_pending_keys=*/false);
    observer->OnEncryptedTypesChanged(state.GetEncryptedTypes(),
                                      /*encrypt_everything=*/true);
    observer->OnPassphraseAccepted();
  }

  void OnFailure(SyncEncryptionHandler::Observer* observer) override {}

 private:
  const std::string passphrase_;
  const KeyDerivationParams key_derivation_params_;
};

class KeystoreInitializer : public PendingLocalNigoriCommit {
 public:
  KeystoreInitializer() {
    cross_user_sharing_public_private_key_pair_ =
        CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair();
  }

  KeystoreInitializer(const KeystoreInitializer&) = delete;
  KeystoreInitializer& operator=(const KeystoreInitializer&) = delete;

  ~KeystoreInitializer() override = default;

  bool TryApply(NigoriState* state) const override {
    DCHECK(!state->keystore_keys_cryptographer->IsEmpty());
    if (state->passphrase_type != NigoriSpecifics::UNKNOWN) {
      return false;
    }

    std::unique_ptr<CryptographerImpl> cryptographer =
        state->keystore_keys_cryptographer->ToCryptographerImpl();
    DCHECK(!cryptographer->GetDefaultEncryptionKeyName().empty());
    state->cryptographer->EmplaceAllNigoriKeysFrom(*cryptographer);
    state->cryptographer->SelectDefaultEncryptionKey(
        cryptographer->GetDefaultEncryptionKeyName());
    state->passphrase_type = NigoriSpecifics::KEYSTORE_PASSPHRASE;
    state->keystore_migration_time = base::Time::Now();

    if (cross_user_sharing_public_private_key_pair_.has_value()) {
      InitNewOrFixCorruptedKeyPair(
          cross_user_sharing_public_private_key_pair_.value(), state);
    }
    return true;
  }

  void OnSuccess(const NigoriState& state,
                 SyncEncryptionHandler::Observer* observer) override {
    // Note: |passphrase_time| isn't populated for keystore passphrase.
    observer->OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase,
                                      /*passphrase_time=*/base::Time());
    observer->OnCryptographerStateChanged(state.cryptographer.get(),
                                          /*has_pending_keys=*/false);
    LogCrossUserSharingPublicPrivateKeyInit(true);
  }

  void OnFailure(SyncEncryptionHandler::Observer* observer) override {
    LogCrossUserSharingPublicPrivateKeyInit(false);
  }

 private:
  std::optional<CrossUserSharingPublicPrivateKeyPair>
      cross_user_sharing_public_private_key_pair_;
};

class KeystoreReencryptor : public PendingLocalNigoriCommit {
 public:
  KeystoreReencryptor() = default;

  KeystoreReencryptor(const KeystoreReencryptor&) = delete;
  KeystoreReencryptor& operator=(const KeystoreReencryptor&) = delete;

  ~KeystoreReencryptor() override = default;

  bool TryApply(NigoriState* state) const override {
    if (!state->NeedsKeystoreReencryption()) {
      return false;
    }
    const std::string new_default_key_name = state->cryptographer->EmplaceKey(
        state->keystore_keys_cryptographer->keystore_keys().back(),
        KeyDerivationParams::CreateForPbkdf2());
    state->cryptographer->SelectDefaultEncryptionKey(new_default_key_name);
    return true;
  }

  void OnSuccess(const NigoriState& state,
                 SyncEncryptionHandler::Observer* observer) override {
    observer->OnCryptographerStateChanged(state.cryptographer.get(),
                                          /*has_pending_keys=*/false);
  }

  void OnFailure(SyncEncryptionHandler::Observer* observer) override {}
};

class CrossUserSharingPublicPrivateKeyInitializer
    : public PendingLocalNigoriCommit {
 public:
  CrossUserSharingPublicPrivateKeyInitializer()
      : cross_user_sharing_public_private_key_pair_(
            CrossUserSharingPublicPrivateKeyPair::GenerateNewKeyPair()) {}

  CrossUserSharingPublicPrivateKeyInitializer(
      const CrossUserSharingPublicPrivateKeyInitializer&) = delete;
  CrossUserSharingPublicPrivateKeyInitializer& operator=(
      const CrossUserSharingPublicPrivateKeyInitializer&) = delete;

  ~CrossUserSharingPublicPrivateKeyInitializer() override = default;

  bool TryApply(NigoriState* state) const override {
    if (!state->NeedsGenerateCrossUserSharingKeyPair()) {
      return false;
    }

    InitNewOrFixCorruptedKeyPair(cross_user_sharing_public_private_key_pair_,
                                 state);
    return true;
  }

  void OnSuccess(const NigoriState& state,
                 SyncEncryptionHandler::Observer* observer) override {
    observer->OnCryptographerStateChanged(state.cryptographer.get(),
                                          /*has_pending_keys=*/false);
    LogCrossUserSharingPublicPrivateKeyInit(true);
  }

  void OnFailure(SyncEncryptionHandler::Observer* observer) override {
    LogCrossUserSharingPublicPrivateKeyInit(false);
  }

 private:
  CrossUserSharingPublicPrivateKeyPair
      cross_user_sharing_public_private_key_pair_;
};

}  // namespace

// static
std::unique_ptr<PendingLocalNigoriCommit>
PendingLocalNigoriCommit::ForSetCustomPassphrase(
    const std::string& passphrase,
    const KeyDerivationParams& key_derivation_params) {
  return std::make_unique<CustomPassphraseSetter>(passphrase,
                                                  key_derivation_params);
}

// static
std::unique_ptr<PendingLocalNigoriCommit>
PendingLocalNigoriCommit::ForKeystoreInitialization() {
  return std::make_unique<KeystoreInitializer>();
}

// static
std::unique_ptr<PendingLocalNigoriCommit>
PendingLocalNigoriCommit::ForKeystoreReencryption() {
  return std::make_unique<KeystoreReencryptor>();
}

// static
std::unique_ptr<PendingLocalNigoriCommit>
PendingLocalNigoriCommit::ForCrossUserSharingPublicPrivateKeyInitializer() {
  return std::make_unique<CrossUserSharingPublicPrivateKeyInitializer>();
}

}  // namespace syncer
