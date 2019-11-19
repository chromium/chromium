// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key.h"

#include "base/base64url.h"
#include "chromeos/services/device_sync/value_string_encoding.h"
#include "crypto/sha2.h"

namespace chromeos {

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
base::Optional<CryptAuthKey> CryptAuthKey::FromDictionary(
    const base::Value& dict) {
  if (!dict.is_dict())
    return base::nullopt;

  base::Optional<int> opt_status = dict.FindIntKey(kStatusDictKey);
  if (!opt_status)
    return base::nullopt;
  CryptAuthKey::Status status = static_cast<CryptAuthKey::Status>(*opt_status);

  base::Optional<int> opt_type = dict.FindIntKey(kTypeDictKey);
  if (!opt_type || !cryptauthv2::KeyType_IsValid(*opt_type))
    return base::nullopt;
  cryptauthv2::KeyType type = static_cast<cryptauthv2::KeyType>(*opt_type);

  const std::string* handle = dict.FindStringKey(kHandleDictKey);
  if (!handle || handle->empty())
    return base::nullopt;

  if (IsSymmetricKeyType(type)) {
    base::Optional<std::string> symmetric_key =
        util::DecodeFromValueString(dict.FindKey(kSymmetricKeyDictKey));
    if (!symmetric_key || symmetric_key->empty())
      return base::nullopt;

    return CryptAuthKey(*symmetric_key, status, type, *handle);
  }

  DCHECK(IsAsymmetricKeyType(type));

  base::Optional<std::string> public_key =
      util::DecodeFromValueString(dict.FindKey(kPublicKeyDictKey));
  base::Optional<std::string> private_key =
      util::DecodeFromValueString(dict.FindKey(kPrivateKeyDictKey));
  if (!public_key || !private_key || public_key->empty()) {
    return base::nullopt;
  }

  return CryptAuthKey(*public_key, *private_key, status, type, *handle);
}

CryptAuthKey::CryptAuthKey(const std::string& symmetric_key,
                           Status status,
                           cryptauthv2::KeyType type,
                           const base::Optional<std::string>& handle)
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
                           const base::Optional<std::string>& handle)
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

base::Value CryptAuthKey::AsSymmetricKeyDictionary() const {
  DCHECK(IsSymmetricKey());

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kHandleDictKey, base::Value(handle_));
  dict.SetKey(kStatusDictKey, base::Value(status_));
  dict.SetKey(kTypeDictKey, base::Value(type_));
  dict.SetKey(kSymmetricKeyDictKey, util::EncodeAsValueString(symmetric_key_));

  return dict;
}

base::Value CryptAuthKey::AsAsymmetricKeyDictionary() const {
  DCHECK(IsAsymmetricKey());

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey(kHandleDictKey, base::Value(handle_));
  dict.SetKey(kStatusDictKey, base::Value(status_));
  dict.SetKey(kTypeDictKey, base::Value(type_));
  dict.SetKey(kPublicKeyDictKey, util::EncodeAsValueString(public_key_));
  dict.SetKey(kPrivateKeyDictKey, util::EncodeAsValueString(private_key_));

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

}  // namespace chromeos
