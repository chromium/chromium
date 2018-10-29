// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/cryptographer.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "components/sync/base/encryptor.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

const char kNigoriTag[] = "google_chrome_nigori";

// We name a particular Nigori instance (ie. a triplet consisting of a hostname,
// a username, and a password) by calling Permute on this string. Since the
// output of Permute is always the same for a given triplet, clients will always
// assign the same name to a particular triplet.
const char kNigoriKeyName[] = "nigori-key";

KeyParams::KeyParams(KeyDerivationParams derivation_params,
                     const std::string& password)
    : derivation_params(derivation_params), password(password) {}

KeyParams::KeyParams(const KeyParams& other) = default;
KeyParams::KeyParams(KeyParams&& other) = default;
KeyParams::~KeyParams() = default;

Cryptographer::Cryptographer(Encryptor* encryptor) : encryptor_(encryptor) {
  DCHECK(encryptor);
}

Cryptographer::Cryptographer(const Cryptographer& other)
    : encryptor_(other.encryptor_),
      default_nigori_name_(other.default_nigori_name_) {
  for (auto it = other.nigoris_.begin(); it != other.nigoris_.end(); ++it) {
    std::string user_key, encryption_key, mac_key;
    it->second->ExportKeys(&user_key, &encryption_key, &mac_key);
    auto nigori_copy = std::make_unique<Nigori>();
    nigori_copy->InitByImport(user_key, encryption_key, mac_key);
    nigoris_.emplace(it->first, std::move(nigori_copy));
  }

  if (other.pending_keys_) {
    pending_keys_ =
        std::make_unique<sync_pb::EncryptedData>(*(other.pending_keys_));
  }
}

Cryptographer::~Cryptographer() {}

void Cryptographer::Bootstrap(const std::string& restored_bootstrap_token) {
  if (is_initialized()) {
    NOTREACHED();
    return;
  }

  std::string serialized_nigori_key =
      UnpackBootstrapToken(restored_bootstrap_token);
  if (serialized_nigori_key.empty())
    return;
  ImportNigoriKey(serialized_nigori_key);
}

bool Cryptographer::CanDecrypt(const sync_pb::EncryptedData& data) const {
  return nigoris_.end() != nigoris_.find(data.key_name());
}

bool Cryptographer::CanDecryptUsingDefaultKey(
    const sync_pb::EncryptedData& data) const {
  return !default_nigori_name_.empty() &&
         data.key_name() == default_nigori_name_;
}

bool Cryptographer::Encrypt(const ::google::protobuf::MessageLite& message,
                            sync_pb::EncryptedData* encrypted) const {
  DCHECK(encrypted);
  if (default_nigori_name_.empty()) {
    LOG(ERROR) << "Cryptographer not ready, failed to encrypt.";
    return false;
  }

  std::string serialized;
  if (!message.SerializeToString(&serialized)) {
    LOG(ERROR) << "Message is invalid/missing a required field.";
    return false;
  }

  return EncryptString(serialized, encrypted);
}

bool Cryptographer::EncryptString(const std::string& serialized,
                                  sync_pb::EncryptedData* encrypted) const {
  if (CanDecryptUsingDefaultKey(*encrypted)) {
    const std::string& original_serialized = DecryptToString(*encrypted);
    if (original_serialized == serialized) {
      DVLOG(2) << "Re-encryption unnecessary, encrypted data already matches.";
      return true;
    }
  }

  auto default_nigori = nigoris_.find(default_nigori_name_);
  if (default_nigori == nigoris_.end()) {
    LOG(ERROR) << "Corrupt default key.";
    return false;
  }

  encrypted->set_key_name(default_nigori_name_);
  if (!default_nigori->second->Encrypt(serialized, encrypted->mutable_blob())) {
    LOG(ERROR) << "Failed to encrypt data.";
    return false;
  }
  return true;
}

bool Cryptographer::Decrypt(const sync_pb::EncryptedData& encrypted,
                            ::google::protobuf::MessageLite* message) const {
  DCHECK(message);
  std::string plaintext = DecryptToString(encrypted);
  return message->ParseFromString(plaintext);
}

std::string Cryptographer::DecryptToString(
    const sync_pb::EncryptedData& encrypted) const {
  auto it = nigoris_.find(encrypted.key_name());
  if (nigoris_.end() == it) {
    // The key used to encrypt the blob is not part of the set of installed
    // nigoris.
    LOG(ERROR) << "Cannot decrypt message";
    return std::string();
  }

  std::string plaintext;
  if (!it->second->Decrypt(encrypted.blob(), &plaintext)) {
    return std::string();
  }

  return plaintext;
}

bool Cryptographer::GetKeys(sync_pb::EncryptedData* encrypted) const {
  DCHECK(encrypted);
  DCHECK(!nigoris_.empty());

  // Create a bag of all the Nigori parameters we know about.
  sync_pb::NigoriKeyBag bag;
  for (const auto& key_name_and_nigori : nigoris_) {
    const Nigori& nigori = *key_name_and_nigori.second;
    sync_pb::NigoriKey* key = bag.add_key();
    key->set_name(key_name_and_nigori.first);
    nigori.ExportKeys(key->mutable_user_key(), key->mutable_encryption_key(),
                      key->mutable_mac_key());
  }

  // Encrypt the bag with the default Nigori.
  return Encrypt(bag, encrypted);
}

bool Cryptographer::AddKey(const KeyParams& params) {
  // Create the new Nigori and make it the default encryptor.
  std::unique_ptr<Nigori> nigori(new Nigori);
  if (!nigori->InitByDerivation(params.derivation_params, params.password)) {
    NOTREACHED();  // Invalid username or password.
    return false;
  }
  return AddKeyImpl(std::move(nigori), true);
}

bool Cryptographer::AddNonDefaultKey(const KeyParams& params) {
  DCHECK(is_initialized());
  // Create the new Nigori and add it to the keybag.
  std::unique_ptr<Nigori> nigori(new Nigori);
  if (!nigori->InitByDerivation(params.derivation_params, params.password)) {
    NOTREACHED();  // Invalid username or password.
    return false;
  }
  return AddKeyImpl(std::move(nigori), false);
}

bool Cryptographer::AddKeyFromBootstrapToken(
    const std::string& restored_bootstrap_token) {
  // Create the new Nigori and make it the default encryptor.
  std::string serialized_nigori_key =
      UnpackBootstrapToken(restored_bootstrap_token);
  return ImportNigoriKey(serialized_nigori_key);
}

bool Cryptographer::AddKeyImpl(std::unique_ptr<Nigori> initialized_nigori,
                               bool set_as_default) {
  std::string name;
  if (!initialized_nigori->Permute(Nigori::Password, kNigoriKeyName, &name)) {
    NOTREACHED();
    return false;
  }

  nigoris_[name] = std::move(initialized_nigori);

  // Check if the key we just added can decrypt the pending keys and add them
  // too if so.
  if (pending_keys_.get() && CanDecrypt(*pending_keys_)) {
    sync_pb::NigoriKeyBag pending_bag;
    Decrypt(*pending_keys_, &pending_bag);
    InstallKeyBag(pending_bag);
    SetDefaultKey(pending_keys_->key_name());
    pending_keys_.reset();
  }

  // The just-added key takes priority over the pending keys as default.
  if (set_as_default)
    SetDefaultKey(name);
  return true;
}

void Cryptographer::InstallKeys(const sync_pb::EncryptedData& encrypted) {
  DCHECK(CanDecrypt(encrypted));

  sync_pb::NigoriKeyBag bag;
  if (!Decrypt(encrypted, &bag))
    return;
  InstallKeyBag(bag);
}

void Cryptographer::SetDefaultKey(const std::string& key_name) {
  DCHECK(nigoris_.end() != nigoris_.find(key_name));
  default_nigori_name_ = key_name;
}

void Cryptographer::SetPendingKeys(const sync_pb::EncryptedData& encrypted) {
  DCHECK(!CanDecrypt(encrypted));
  DCHECK(!encrypted.blob().empty());
  pending_keys_ = std::make_unique<sync_pb::EncryptedData>(encrypted);
}

const sync_pb::EncryptedData& Cryptographer::GetPendingKeys() const {
  DCHECK(has_pending_keys());
  return *(pending_keys_.get());
}

bool Cryptographer::DecryptPendingKeys(const KeyParams& params) {
  Nigori nigori;
  if (!nigori.InitByDerivation(params.derivation_params, params.password)) {
    NOTREACHED();
    return false;
  }

  std::string plaintext;
  if (!nigori.Decrypt(pending_keys_->blob(), &plaintext))
    return false;

  sync_pb::NigoriKeyBag bag;
  if (!bag.ParseFromString(plaintext)) {
    NOTREACHED();
    return false;
  }
  InstallKeyBag(bag);
  const std::string& new_default_key_name = pending_keys_->key_name();
  SetDefaultKey(new_default_key_name);
  pending_keys_.reset();
  return true;
}

bool Cryptographer::GetBootstrapToken(std::string* token) const {
  DCHECK(token);
  std::string unencrypted_token = GetDefaultNigoriKeyData();
  if (unencrypted_token.empty())
    return false;

  std::string encrypted_token;
  if (!encryptor_->EncryptString(unencrypted_token, &encrypted_token)) {
    return false;
  }

  base::Base64Encode(encrypted_token, token);

  return true;
}

std::string Cryptographer::UnpackBootstrapToken(
    const std::string& token) const {
  if (token.empty())
    return std::string();

  std::string encrypted_data;
  if (!base::Base64Decode(token, &encrypted_data)) {
    DLOG(WARNING) << "Could not decode token.";
    return std::string();
  }

  std::string unencrypted_token;
  if (!encryptor_->DecryptString(encrypted_data, &unencrypted_token)) {
    DLOG(WARNING) << "Decryption of bootstrap token failed.";
    return std::string();
  }
  return unencrypted_token;
}

void Cryptographer::InstallKeyBag(const sync_pb::NigoriKeyBag& bag) {
  int key_size = bag.key_size();
  for (int i = 0; i < key_size; ++i) {
    const sync_pb::NigoriKey key = bag.key(i);
    // Only use this key if we don't already know about it.
    if (nigoris_.end() == nigoris_.find(key.name())) {
      std::unique_ptr<Nigori> new_nigori(new Nigori);
      if (!new_nigori->InitByImport(key.user_key(), key.encryption_key(),
                                    key.mac_key())) {
        NOTREACHED();
        continue;
      }
      nigoris_[key.name()] = std::move(new_nigori);
    }
  }
}

bool Cryptographer::KeybagIsStale(
    const sync_pb::EncryptedData& encrypted_bag) const {
  if (!is_ready())
    return false;
  if (encrypted_bag.blob().empty())
    return true;
  if (!CanDecrypt(encrypted_bag))
    return false;
  if (!CanDecryptUsingDefaultKey(encrypted_bag))
    return true;
  sync_pb::NigoriKeyBag bag;
  if (!Decrypt(encrypted_bag, &bag)) {
    LOG(ERROR) << "Failed to decrypt keybag for stale check. "
               << "Assuming keybag is corrupted.";
    return true;
  }
  if (static_cast<size_t>(bag.key_size()) < nigoris_.size())
    return true;
  return false;
}

std::string Cryptographer::GetDefaultNigoriKeyName() const {
  return default_nigori_name_;
}

std::string Cryptographer::GetDefaultNigoriKeyData() const {
  if (!is_initialized())
    return std::string();
  auto iter = nigoris_.find(default_nigori_name_);
  if (iter == nigoris_.end())
    return std::string();
  sync_pb::NigoriKey key;
  iter->second->ExportKeys(key.mutable_user_key(), key.mutable_encryption_key(),
                           key.mutable_mac_key());
  return key.SerializeAsString();
}

bool Cryptographer::ImportNigoriKey(const std::string& serialized_nigori_key) {
  if (serialized_nigori_key.empty())
    return false;

  sync_pb::NigoriKey key;
  if (!key.ParseFromString(serialized_nigori_key))
    return false;

  std::unique_ptr<Nigori> nigori(new Nigori);
  if (!nigori->InitByImport(key.user_key(), key.encryption_key(),
                            key.mac_key())) {
    NOTREACHED();
    return false;
  }

  if (!AddKeyImpl(std::move(nigori), true))
    return false;
  return true;
}

}  // namespace syncer
