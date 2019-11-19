// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/keystore_keys_cryptographer.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

// static
std::unique_ptr<KeystoreKeysCryptographer>
KeystoreKeysCryptographer::CreateEmpty() {
  return base::WrapUnique(new KeystoreKeysCryptographer(
      CryptographerImpl::CreateEmpty(),
      /*keystore_keys=*/std::vector<std::string>()));
}

// static
std::unique_ptr<KeystoreKeysCryptographer>
KeystoreKeysCryptographer::FromKeystoreKeys(
    const std::vector<std::string>& keystore_keys) {
  if (keystore_keys.empty()) {
    return CreateEmpty();
  }

  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();

  std::string last_key_name;

  for (const std::string& key : keystore_keys) {
    last_key_name =
        cryptographer->EmplaceKey(key, KeyDerivationParams::CreateForPbkdf2());
    // TODO(crbug.com/922900): possible behavioral change. Old implementation
    // fails only if we failed to add current keystore key. Failing to add any
    // of these keys doesn't seem valid. This line seems to be a good candidate
    // for UMA, as it's not a normal situation, if we fail to add any key.
    if (last_key_name.empty()) {
      return nullptr;
    }
  }

  DCHECK(!last_key_name.empty());
  cryptographer->SelectDefaultEncryptionKey(last_key_name);

  return base::WrapUnique(
      new KeystoreKeysCryptographer(std::move(cryptographer), keystore_keys));
}

KeystoreKeysCryptographer::KeystoreKeysCryptographer(
    std::unique_ptr<CryptographerImpl> cryptographer,
    const std::vector<std::string>& keystore_keys)
    : cryptographer_(std::move(cryptographer)), keystore_keys_(keystore_keys) {
  DCHECK(cryptographer_);
}

KeystoreKeysCryptographer::~KeystoreKeysCryptographer() = default;

std::string KeystoreKeysCryptographer::GetLastKeystoreKeyName() const {
  return cryptographer_->GetDefaultEncryptionKeyName();
}

bool KeystoreKeysCryptographer::IsEmpty() const {
  return keystore_keys_.empty();
}

std::unique_ptr<KeystoreKeysCryptographer> KeystoreKeysCryptographer::Clone()
    const {
  return base::WrapUnique(new KeystoreKeysCryptographer(
      cryptographer_->CloneImpl(), keystore_keys_));
}

std::unique_ptr<CryptographerImpl>
KeystoreKeysCryptographer::ToCryptographerImpl() const {
  return cryptographer_->CloneImpl();
}

bool KeystoreKeysCryptographer::EncryptKeystoreDecryptorToken(
    const sync_pb::NigoriKey& keystore_decryptor_key,
    sync_pb::EncryptedData* keystore_decryptor_token) const {
  DCHECK(keystore_decryptor_token);
  if (IsEmpty()) {
    return false;
  }
  return cryptographer_->EncryptString(
      keystore_decryptor_key.SerializeAsString(), keystore_decryptor_token);
}

bool KeystoreKeysCryptographer::DecryptKeystoreDecryptorToken(
    const sync_pb::EncryptedData& keystore_decryptor_token,
    sync_pb::NigoriKey* keystore_decryptor_key) const {
  return cryptographer_->Decrypt(keystore_decryptor_token,
                                 keystore_decryptor_key);
}

}  // namespace syncer
