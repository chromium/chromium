// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/pending_local_nigori_commit.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/keystore_keys_cryptographer.h"
#include "components/sync/nigori/nigori_state.h"

namespace syncer {

namespace {

using sync_pb::NigoriSpecifics;

// Returns the key derivation method to be used when a user sets a new
// custom passphrase.
KeyDerivationMethod GetDefaultKeyDerivationMethodForCustomPassphrase() {
  if (base::FeatureList::IsEnabled(
          switches::kSyncUseScryptForNewCustomPassphrases) &&
      !base::FeatureList::IsEnabled(
          switches::kSyncForceDisableScryptForCustomPassphrase)) {
    return KeyDerivationMethod::SCRYPT_8192_8_11;
  }

  return KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003;
}

class CustomPassphraseSetter : public PendingLocalNigoriCommit {
 public:
  CustomPassphraseSetter(
      const std::string& passphrase,
      const base::RepeatingCallback<std::string()>& random_salt_generator)
      : passphrase_(passphrase),
        key_derivation_params_(CreateKeyDerivationParamsForCustomPassphrase(
            random_salt_generator)) {}

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

    observer->OnPassphraseAccepted();
    observer->OnPassphraseTypeChanged(PassphraseType::kCustomPassphrase,
                                      state.custom_passphrase_time);
    observer->OnCryptographerStateChanged(state.cryptographer.get(),
                                          /*has_pending_keys=*/false);
    observer->OnEncryptedTypesChanged(EncryptableUserTypes(),
                                      /*encrypt_everything=*/true);

    UMA_HISTOGRAM_BOOLEAN("Sync.CustomEncryption", true);

    // OnLocalSetPassphraseEncryption() is intentionally not called here,
    // because it's needed only for the Directory implementation unit tests.
  }

  void OnFailure(SyncEncryptionHandler::Observer* observer) override {
    // TODO(crbug.com/922900): investigate whether we need to call
    // OnPassphraseRequired() to prompt for decryption passphrase.
  }

 private:
  const std::string passphrase_;
  const KeyDerivationParams key_derivation_params_;

  DISALLOW_COPY_AND_ASSIGN(CustomPassphraseSetter);
};

class KeystoreInitializer : public PendingLocalNigoriCommit {
 public:
  KeystoreInitializer() = default;
  ~KeystoreInitializer() override = default;

  bool TryApply(NigoriState* state) const override {
    DCHECK(!state->keystore_keys_cryptographer->IsEmpty());
    if (state->passphrase_type != NigoriSpecifics::UNKNOWN) {
      return false;
    }

    state->passphrase_type = NigoriSpecifics::KEYSTORE_PASSPHRASE;
    state->keystore_migration_time = base::Time::Now();
    state->cryptographer =
        state->keystore_keys_cryptographer->ToCryptographerImpl();
    return true;
  }

  void OnSuccess(const NigoriState& state,
                 SyncEncryptionHandler::Observer* observer) override {
    // Note: |passphrase_time| isn't populated for keystore passphrase.
    observer->OnPassphraseTypeChanged(PassphraseType::kKeystorePassphrase,
                                      /*passphrase_time=*/base::Time());
    observer->OnCryptographerStateChanged(state.cryptographer.get(),
                                          /*has_pending_keys=*/false);
  }

  void OnFailure(SyncEncryptionHandler::Observer* observer) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(KeystoreInitializer);
};

class KeystoreKeyRotator : public PendingLocalNigoriCommit {
 public:
  KeystoreKeyRotator() = default;
  ~KeystoreKeyRotator() override = default;

  bool TryApply(NigoriState* state) const override {
    if (!state->NeedsKeystoreKeyRotation()) {
      return false;
    }
    // TODO(crbug.com/922900): ensure that |cryptographer| contains all
    // keystore keys? (In theory it's safe to add only last one).
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

 private:
  DISALLOW_COPY_AND_ASSIGN(KeystoreKeyRotator);
};

}  // namespace

KeyDerivationParams CreateKeyDerivationParamsForCustomPassphrase(
    const base::RepeatingCallback<std::string()>& random_salt_generator) {
  KeyDerivationMethod method =
      GetDefaultKeyDerivationMethodForCustomPassphrase();
  switch (method) {
    case KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      return KeyDerivationParams::CreateForPbkdf2();
    case KeyDerivationMethod::SCRYPT_8192_8_11:
      return KeyDerivationParams::CreateForScrypt(random_salt_generator.Run());
    case KeyDerivationMethod::UNSUPPORTED:
      break;
  }

  NOTREACHED();
  return KeyDerivationParams::CreateWithUnsupportedMethod();
}

// static
std::unique_ptr<PendingLocalNigoriCommit>
PendingLocalNigoriCommit::ForSetCustomPassphrase(
    const std::string& passphrase,
    const base::RepeatingCallback<std::string()>& random_salt_generator) {
  return std::make_unique<CustomPassphraseSetter>(passphrase,
                                                  random_salt_generator);
}

// static
std::unique_ptr<PendingLocalNigoriCommit>
PendingLocalNigoriCommit::ForKeystoreInitialization() {
  return std::make_unique<KeystoreInitializer>();
}

// static
std::unique_ptr<PendingLocalNigoriCommit>
PendingLocalNigoriCommit::ForKeystoreKeyRotation() {
  return std::make_unique<KeystoreKeyRotator>();
}

}  // namespace syncer
