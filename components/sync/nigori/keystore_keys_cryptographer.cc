// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/keystore_keys_cryptographer.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "components/sync/engine/nigori/key_derivation_params.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/nigori_key_bag.h"
#include "components/sync/protocol/encryption.pb.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

// static
std::unique_ptr<KeystoreKeysCryptographer>
KeystoreKeysCryptographer::CreateEmpty() {
  return base::WrapUnique(new KeystoreKeysCryptographer(
      NigoriKeyBag::CreateEmpty(),
      /*last_keystore_key_name=*/std::string(),
      /*keystore_keys=*/std::vector<std::string>()));
}

// static
std::unique_ptr<KeystoreKeysCryptographer>
KeystoreKeysCryptographer::FromKeystoreKeys(
    const std::vector<std::string>& keystore_keys) {
  if (keystore_keys.empty()) {
    return CreateEmpty();
  }

  NigoriKeyBag key_bag = NigoriKeyBag::CreateEmpty();
  std::string last_key_name;

  for (const std::string& key : keystore_keys) {
    last_key_name = key_bag.AddKey(Nigori::CreateByDerivation(
        KeyDerivationParams::CreateForPbkdf2(), key));

    if (last_key_name.empty()) {
      // TODO(crbug.com/40868132): this shouldn't be possible, clean up once
      // lower-level Nigori code explicitly guarantees that.
      return nullptr;
    }
  }

  DCHECK(!last_key_name.empty());

  return base::WrapUnique(new KeystoreKeysCryptographer(
      std::move(key_bag), last_key_name, keystore_keys));
}

KeystoreKeysCryptographer::KeystoreKeysCryptographer(
    NigoriKeyBag key_bag,
    const std::string& last_keystore_key_name,
    const std::vector<std::string>& keystore_keys)
    : key_bag_(std::move(key_bag)),
      last_keystore_key_name_(last_keystore_key_name),
      keystore_keys_(keystore_keys) {}

KeystoreKeysCryptographer::~KeystoreKeysCryptographer() = default;

std::string KeystoreKeysCryptographer::GetLastKeystoreKeyName() const {
  return last_keystore_key_name_;
}

bool KeystoreKeysCryptographer::IsEmpty() const {
  return keystore_keys_.empty();
}

std::unique_ptr<KeystoreKeysCryptographer> KeystoreKeysCryptographer::Clone()
    const {
  return base::WrapUnique(new KeystoreKeysCryptographer(
      key_bag_.Clone(), last_keystore_key_name_, keystore_keys_));
}

std::unique_ptr<CryptographerImpl>
KeystoreKeysCryptographer::ToCryptographerImpl() const {
  std::unique_ptr<CryptographerImpl> cryptographer =
      CryptographerImpl::CreateEmpty();
  cryptographer->EmplaceKeysFrom(key_bag_);
  if (!last_keystore_key_name_.empty()) {
    cryptographer->SelectDefaultEncryptionKey(last_keystore_key_name_);
  }
  return cryptographer;
}

bool KeystoreKeysCryptographer::EncryptKeystoreDecryptorToken(
    const sync_pb::NigoriKey& keystore_decryptor_key,
    sync_pb::EncryptedData* keystore_decryptor_token) const {
  CHECK(keystore_decryptor_token);
  if (IsEmpty()) {
    return false;
  }

  *keystore_decryptor_token = key_bag_.EncryptWithKey(
      last_keystore_key_name_, keystore_decryptor_key.SerializeAsString());
  return true;
}

bool KeystoreKeysCryptographer::DecryptKeystoreDecryptorToken(
    const sync_pb::EncryptedData& keystore_decryptor_token,
    sync_pb::NigoriKey* keystore_decryptor_key) const {
  std::string serialized_keystore_decryptor_key;
  if (!key_bag_.Decrypt(keystore_decryptor_token,
                        &serialized_keystore_decryptor_key)) {
    return false;
  }
  return keystore_decryptor_key->ParseFromString(
      serialized_keystore_decryptor_key);
}

const NigoriKeyBag& KeystoreKeysCryptographer::GetKeystoreKeybag() const {
  return key_bag_;
}

}  // namespace syncer
