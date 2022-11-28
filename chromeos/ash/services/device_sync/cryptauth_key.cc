// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_key.h"

#include "base/base64url.h"
#include "chromeos/ash/services/device_sync/value_string_encoding.h"
#include "crypto/sha2.h"

namespace ash {

namespace device_sync {

namespace {

// Strings used as the DictionaryValue keys in As*DictionaryValue().
const char kHandleDictKey[] = "handle";
const char kStatusDictKey[] = "status";
const char kTypeDictKey[] = "type";
const char kSymmetricKeyDictKey[] = "symmetric_key";
const char kPublicKeyDictKey[] = "public_key";
const char kPrivateKeyDictKey[] = "private_key";

// Returns the base64url-encoded SHA256 hash of the input string.
std::string CreateHandle(const std::string& string_to_hash) {
  std::string handle;
  base::Base64UrlEncode(crypto::SHA256HashString(string_to_hash),
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING, &handle);
  return handle;
}

bool IsSymmetricKeyType(cryptauthv2::KeyType type) {
  return (type == cryptauthv2::KeyType::RAW128 ||
          type == cryptauthv2::KeyType::RAW256);
}

bool IsAsymmetricKeyType(cryptauthv2::KeyType type) {
  return (type == cryptauthv2::KeyType::P256 ||
          type == cryptauthv2::KeyType::CURVE25519);
}

}  // namespace

// static
absl::optional<CryptAuthKey> CryptAuthKey::FromDictionary(
    const base::Value& value) {
  if (!value.is_dict())
    return absl::nullopt;

  const base::Value::Dict& dict = value.GetDict();
  absl::optional<int> opt_status = dict.FindInt(kStatusDictKey);
  if (!opt_status)
    return absl::nullopt;
  CryptAuthKey::Status status = static_cast<CryptAuthKey::Status>(*opt_status);

  absl::optional<int> opt_type = dict.FindInt(kTypeDictKey);
  if (!opt_type || !cryptauthv2::KeyType_IsValid(*opt_type))
    return absl::nullopt;
  cryptauthv2::KeyType type = static_cast<cryptauthv2::KeyType>(*opt_type);

  const std::string* handle = dict.FindString(kHandleDictKey);
  if (!handle || handle->empty())
    return absl::nullopt;

  if (IsSymmetricKeyType(type)) {
    absl::optional<std::string> symmetric_key =
        util::DecodeFromValueString(dict.Find(kSymmetricKeyDictKey));
    if (!symmetric_key || symmetric_key->empty())
      return absl::nullopt;

    return CryptAuthKey(*symmetric_key, status, type, *handle);
  }

  DCHECK(IsAsymmetricKeyType(type));

  absl::optional<std::string> public_key =
      util::DecodeFromValueString(dict.Find(kPublicKeyDictKey));
  absl::optional<std::string> private_key =
      util::DecodeFromValueString(dict.Find(kPrivateKeyDictKey));
  if (!public_key || !private_key || public_key->empty()) {
    return absl::nullopt;
  }

  return CryptAuthKey(*public_key, *private_key, status, type, *handle);
}

CryptAuthKey::CryptAuthKey(const std::string& symmetric_key,
                           Status status,
                           cryptauthv2::KeyType type,
                           const absl::optional<std::string>& handle)
    : symmetric_key_(symmetric_key),
      status_(status),
      type_(type),
      handle_(handle ? *handle : CreateHandle(symmetric_key)) {
  DCHECK(IsSymmetricKey());
  DCHECK(!symmetric_key_.empty());
  DCHECK(!handle_.empty());
}

CryptAuthKey::CryptAuthKey(const std::string& public_key,
                           const std::string& private_key,
                           Status status,
                           cryptauthv2::KeyType type,
                           const absl::optional<std::string>& handle)
    : public_key_(public_key),
      private_key_(private_key),
      status_(status),
      type_(type),
      handle_(handle ? *handle : CreateHandle(public_key)) {
  DCHECK(IsAsymmetricKey());
  DCHECK(!public_key_.empty());
  DCHECK(!handle_.empty());
}

CryptAuthKey::CryptAuthKey(const CryptAuthKey&) = default;

CryptAuthKey::~CryptAuthKey() = default;

bool CryptAuthKey::IsSymmetricKey() const {
  return IsSymmetricKeyType(type_);
}

bool CryptAuthKey::IsAsymmetricKey() const {
  return IsAsymmetricKeyType(type_);
}

base::Value::Dict CryptAuthKey::AsSymmetricKeyDictionary() const {
  DCHECK(IsSymmetricKey());

  base::Value::Dict dict;
  dict.Set(kHandleDictKey, handle_);
  dict.Set(kStatusDictKey, status_);
  dict.Set(kTypeDictKey, type_);
  dict.Set(kSymmetricKeyDictKey, util::EncodeAsValueString(symmetric_key_));

  return dict;
}

base::Value::Dict CryptAuthKey::AsAsymmetricKeyDictionary() const {
  DCHECK(IsAsymmetricKey());

  base::Value::Dict dict;
  dict.Set(kHandleDictKey, handle_);
  dict.Set(kStatusDictKey, status_);
  dict.Set(kTypeDictKey, type_);
  dict.Set(kPublicKeyDictKey, util::EncodeAsValueString(public_key_));
  dict.Set(kPrivateKeyDictKey, util::EncodeAsValueString(private_key_));

  return dict;
}

bool CryptAuthKey::operator==(const CryptAuthKey& other) const {
  return handle_ == other.handle_ && status_ == other.status_ &&
         type_ == other.type_ && symmetric_key_ == other.symmetric_key_ &&
         public_key_ == other.public_key_ && private_key_ == other.private_key_;
}

bool CryptAuthKey::operator!=(const CryptAuthKey& other) const {
  return !(*this == other);
}

}  // namespace device_sync

}  // namespace ash
