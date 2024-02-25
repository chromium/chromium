// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/sync_trusted_vault_keys.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"

namespace ash {

namespace {

// Keys in base::Value dictionaries.
const char kEncryptionKeysDictKey[] = "encryptionKeys";
const char kGaiaIdDictKey[] = "obfuscatedGaiaId";
const char kKeyMaterialDictKey[] = "keyMaterial";
const char kMethodTypeHintDictKey[] = "type";
const char kPublicKeyDictKey[] = "publicKey";
const char kTrustedRecoveryMethodsDictKey[] = "trustedRecoveryMethods";
const char kVersionDictKey[] = "version";

struct KeyMaterialAndVersion {
  std::vector<uint8_t> key_material;
  int version = 0;
};

std::optional<KeyMaterialAndVersion> ParseSingleEncryptionKey(
    const base::Value::Dict& js_object) {
  const base::Value::BlobStorage* key_material =
      js_object.FindBlob(kKeyMaterialDictKey);
  if (key_material == nullptr) {
    return std::nullopt;
  }

  return KeyMaterialAndVersion{*key_material,
                               js_object.FindInt(kVersionDictKey).value_or(0)};
}

std::optional<SyncTrustedVaultKeys::TrustedRecoveryMethod>
ParseSingleTrustedRecoveryMethod(const base::Value::Dict& js_object) {
  const base::Value::BlobStorage* public_key =
      js_object.FindBlob(kPublicKeyDictKey);
  if (public_key == nullptr) {
    return std::nullopt;
  }

  SyncTrustedVaultKeys::TrustedRecoveryMethod method;
  method.public_key = *public_key;
  method.type_hint = js_object.FindInt(kMethodTypeHintDictKey).value_or(0);
  return method;
}

template <typename T>
std::vector<T> ParseList(
    const base::Value::List* list,
    const base::RepeatingCallback<std::optional<T>(const base::Value::Dict&)>&
        entry_parser) {
  if (list == nullptr) {
    return {};
  }

  std::vector<T> parsed_list;
  for (const base::Value& list_entry : *list) {
    std::optional<T> parsed_entry = entry_parser.Run(list_entry.GetDict());
    if (parsed_entry.has_value()) {
      parsed_list.push_back(std::move(*parsed_entry));
    }
  }

  return parsed_list;
}

}  // namespace

SyncTrustedVaultKeys::TrustedRecoveryMethod::TrustedRecoveryMethod() = default;

SyncTrustedVaultKeys::TrustedRecoveryMethod::TrustedRecoveryMethod(
    const TrustedRecoveryMethod&) = default;

SyncTrustedVaultKeys::TrustedRecoveryMethod&
SyncTrustedVaultKeys::TrustedRecoveryMethod::operator=(
    const TrustedRecoveryMethod&) = default;

SyncTrustedVaultKeys::TrustedRecoveryMethod::~TrustedRecoveryMethod() = default;

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
    const base::Value::Dict& js_object) {
  SyncTrustedVaultKeys result;
  const std::string* gaia_id = js_object.FindString(kGaiaIdDictKey);
  if (gaia_id) {
    result.gaia_id_ = *gaia_id;
  }

  const std::vector<KeyMaterialAndVersion> encryption_keys =
      ParseList(js_object.FindList(kEncryptionKeysDictKey),
                base::BindRepeating(&ParseSingleEncryptionKey));

  for (const KeyMaterialAndVersion& key : encryption_keys) {
    if (key.version != 0) {
      result.encryption_keys_.push_back(key.key_material);
      result.last_encryption_key_version_ = key.version;
    }
  }

  result.trusted_recovery_methods_ =
      ParseList(js_object.FindList(kTrustedRecoveryMethodsDictKey),
                base::BindRepeating(&ParseSingleTrustedRecoveryMethod));

  return result;
}

const std::string& SyncTrustedVaultKeys::gaia_id() const {
  return gaia_id_;
}

const std::vector<std::vector<uint8_t>>& SyncTrustedVaultKeys::encryption_keys()
    const {
  return encryption_keys_;
}

int SyncTrustedVaultKeys::last_encryption_key_version() const {
  return last_encryption_key_version_;
}

const std::vector<SyncTrustedVaultKeys::TrustedRecoveryMethod>&
SyncTrustedVaultKeys::trusted_recovery_methods() const {
  return trusted_recovery_methods_;
}

}  // namespace ash
