// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/sync_trusted_vault_keys.h"

#include "base/optional.h"
#include "base/values.h"

namespace chromeos {

namespace {

// Keys in base::Value dictionaries.
const char kEncryptionKeysDictKey[] = "encryptionKeys";
const char kTrustedPublicKeysDictKey[] = "trustedPublicKeys";
const char kKeyMaterialDictKey[] = "keyMaterial";
const char kVersionDictKey[] = "version";

struct KeyMaterialAndVersion {
  std::vector<uint8_t> key_material;
  int version = 0;
};

base::Optional<KeyMaterialAndVersion> ParseSingleKey(
    const base::Value& js_object) {
  const base::Value::BlobStorage* key_material =
      js_object.FindBlobKey(kKeyMaterialDictKey);
  if (key_material == nullptr) {
    return base::nullopt;
  }

  return KeyMaterialAndVersion{
      *key_material, js_object.FindIntKey(kVersionDictKey).value_or(0)};
}

std::vector<KeyMaterialAndVersion> ParseKeyList(const base::Value* key_list) {
  if (key_list == nullptr || !key_list->is_list()) {
    return {};
  }

  std::vector<KeyMaterialAndVersion> parsed_keys;
  for (const base::Value& key : key_list->GetList()) {
    base::Optional<KeyMaterialAndVersion> parsed_key = ParseSingleKey(key);
    if (parsed_key.has_value()) {
      parsed_keys.push_back(std::move(*parsed_key));
    }
  }
  return parsed_keys;
}

}  // namespace

SyncTrustedVaultKeys::SyncTrustedVaultKeys() = default;

SyncTrustedVaultKeys::SyncTrustedVaultKeys(const SyncTrustedVaultKeys&) =
    default;

SyncTrustedVaultKeys::SyncTrustedVaultKeys(SyncTrustedVaultKeys&&) = default;

SyncTrustedVaultKeys& SyncTrustedVaultKeys::operator=(
    const SyncTrustedVaultKeys&) = default;

SyncTrustedVaultKeys& SyncTrustedVaultKeys::operator=(SyncTrustedVaultKeys&&) =
    default;

SyncTrustedVaultKeys::~SyncTrustedVaultKeys() = default;

// static
SyncTrustedVaultKeys SyncTrustedVaultKeys::FromJs(
    const base::DictionaryValue& js_object) {
  const std::vector<KeyMaterialAndVersion> encryption_keys =
      ParseKeyList(js_object.FindListKey(kEncryptionKeysDictKey));
  const std::vector<KeyMaterialAndVersion> trusted_public_keys =
      ParseKeyList(js_object.FindListKey(kTrustedPublicKeysDictKey));

  SyncTrustedVaultKeys result;
  for (const KeyMaterialAndVersion& key : encryption_keys) {
    if (key.version != 0) {
      result.encryption_keys_.push_back(key.key_material);
      result.last_encryption_key_version_ = key.version;
    }
  }

  for (const KeyMaterialAndVersion& key : trusted_public_keys) {
    result.trusted_public_keys_.push_back(key.key_material);
  }

  return result;
}

const std::vector<std::vector<uint8_t>>& SyncTrustedVaultKeys::encryption_keys()
    const {
  return encryption_keys_;
}

int SyncTrustedVaultKeys::last_encryption_key_version() const {
  return last_encryption_key_version_;
}

const std::vector<std::vector<uint8_t>>&
SyncTrustedVaultKeys::trusted_public_keys() const {
  return trusted_public_keys_;
}

}  // namespace chromeos
