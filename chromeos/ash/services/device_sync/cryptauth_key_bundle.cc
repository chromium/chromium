// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_key_bundle.h"

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/values.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_constants.h"
#include "chromeos/ash/services/device_sync/value_string_encoding.h"

namespace ash::device_sync {

namespace {

const char kDeviceSyncBetterTogetherGroupKeyName[] =
    "DeviceSyncBetterTogetherGroupKey";
const char kBundleNameDictKey[] = "name";
const char kKeyListDictKey[] = "keys";
const char kKeyDirectiveDictKey[] = "key_directive";

}  // namespace

// static
const base::flat_set<CryptAuthKeyBundle::Name>& CryptAuthKeyBundle::AllNames() {
  static const base::NoDestructor<base::flat_set<CryptAuthKeyBundle::Name>>
      name_list({CryptAuthKeyBundle::Name::kUserKeyPair,
                 CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
                 CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether,
                 CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey});
  return *name_list;
}

const base::flat_set<CryptAuthKeyBundle::Name>&
CryptAuthKeyBundle::AllEnrollableNames() {
  static const base::NoDestructor<base::flat_set<CryptAuthKeyBundle::Name>>
      name_list({CryptAuthKeyBundle::Name::kUserKeyPair,
                 CryptAuthKeyBundle::Name::kLegacyAuthzenKey,
                 CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether});
  return *name_list;
}

// static
std::string CryptAuthKeyBundle::KeyBundleNameEnumToString(
    CryptAuthKeyBundle::Name name) {
  switch (name) {
    case CryptAuthKeyBundle::Name::kUserKeyPair:
      return kCryptAuthUserKeyPairName;
    case CryptAuthKeyBundle::Name::kLegacyAuthzenKey:
      return kCryptAuthLegacyAuthzenKeyName;
    case CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether:
      return kCryptAuthDeviceSyncBetterTogetherKeyName;
    case CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey:
      return kDeviceSyncBetterTogetherGroupKeyName;
  }
}

// static
std::optional<CryptAuthKeyBundle::Name>
CryptAuthKeyBundle::KeyBundleNameStringToEnum(const std::string& name) {
  if (name == kCryptAuthUserKeyPairName)
    return CryptAuthKeyBundle::Name::kUserKeyPair;
  if (name == kCryptAuthLegacyAuthzenKeyName)
    return CryptAuthKeyBundle::Name::kLegacyAuthzenKey;
  if (name == kCryptAuthDeviceSyncBetterTogetherKeyName)
    return CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether;
  if (name == kDeviceSyncBetterTogetherGroupKeyName)
    return CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey;

  return std::nullopt;
}

// static
std::optional<CryptAuthKeyBundle> CryptAuthKeyBundle::FromDictionary(
    const base::Value::Dict& dict) {
  const std::string* name_string = dict.FindString(kBundleNameDictKey);
  if (!name_string)
    return std::nullopt;

  std::optional<CryptAuthKeyBundle::Name> name =
      KeyBundleNameStringToEnum(*name_string);
  if (!name)
    return std::nullopt;

  CryptAuthKeyBundle bundle(*name);

  const base::Value::List* keys = dict.FindList(kKeyListDictKey);
  if (!keys)
    return std::nullopt;

  bool active_key_exists = false;
  for (const base::Value& key_dict : *keys) {
    std::optional<CryptAuthKey> key =
        CryptAuthKey::FromDictionary(key_dict.GetDict());
    if (!key)
      return std::nullopt;

    // Return nullopt if there are multiple active keys.
    if (key->status() == CryptAuthKey::Status::kActive) {
      if (active_key_exists)
        return std::nullopt;

      active_key_exists = true;
    }

    // Return nullopt if duplicate handles exist.
    if (base::Contains(bundle.handle_to_key_map(), key->handle()))
      return std::nullopt;

    bundle.AddKey(*key);
  }

  const base::Value* encoded_serialized_key_directive =
      dict.Find(kKeyDirectiveDictKey);
  if (encoded_serialized_key_directive) {
    std::optional<cryptauthv2::KeyDirective> key_directive =
        util::DecodeProtoMessageFromValueString<cryptauthv2::KeyDirective>(
            encoded_serialized_key_directive);
    if (!key_directive)
      return std::nullopt;

    bundle.set_key_directive(*key_directive);
  }

  return bundle;
}

CryptAuthKeyBundle::CryptAuthKeyBundle(Name name) : name_(name) {}

CryptAuthKeyBundle::CryptAuthKeyBundle(const CryptAuthKeyBundle&) = default;

CryptAuthKeyBundle::~CryptAuthKeyBundle() = default;

const CryptAuthKey* CryptAuthKeyBundle::GetActiveKey() const {
  const auto& it =
      base::ranges::find(handle_to_key_map_, CryptAuthKey::Status::kActive,
                         [](const HandleToKeyMap::value_type& handle_key_pair) {
                           return handle_key_pair.second.status();
                         });

  if (it == handle_to_key_map_.end())
    return nullptr;

  return &it->second;
}

void CryptAuthKeyBundle::AddKey(const CryptAuthKey& key) {
  DCHECK(name_ != Name::kUserKeyPair ||
         key.handle() == kCryptAuthFixedUserKeyPairHandle);

  if (key.status() == CryptAuthKey::Status::kActive)
    DeactivateKeys();

  handle_to_key_map_.insert_or_assign(key.handle(), key);
}

void CryptAuthKeyBundle::SetActiveKey(const std::string& handle) {
  auto it = handle_to_key_map_.find(handle);
  DCHECK(it != handle_to_key_map_.end());
  DeactivateKeys();
  it->second.set_status(CryptAuthKey::Status::kActive);
}

void CryptAuthKeyBundle::DeleteKey(const std::string& handle) {
  DCHECK(base::Contains(handle_to_key_map_, handle));
  handle_to_key_map_.erase(handle);
}

void CryptAuthKeyBundle::DeactivateKeys() {
  for (auto& handle_key_pair : handle_to_key_map_)
    handle_key_pair.second.set_status(CryptAuthKey::Status::kInactive);
}

base::Value::Dict CryptAuthKeyBundle::AsDictionary() const {
  base::Value::Dict dict;

  dict.Set(kBundleNameDictKey, KeyBundleNameEnumToString(name_));

  base::Value::List keys;
  for (const auto& handle_key_pair : handle_to_key_map_) {
    if (handle_key_pair.second.IsSymmetricKey()) {
      keys.Append(handle_key_pair.second.AsSymmetricKeyDictionary());
      continue;
    }

    DCHECK(handle_key_pair.second.IsAsymmetricKey());
    keys.Append(handle_key_pair.second.AsAsymmetricKeyDictionary());
  }
  dict.Set(kKeyListDictKey, std::move(keys));

  if (key_directive_) {
    dict.Set(kKeyDirectiveDictKey,
             util::EncodeProtoMessageAsValueString(&key_directive_.value()));
  }

  return dict;
}

bool CryptAuthKeyBundle::operator==(const CryptAuthKeyBundle& other) const {
  return name_ == other.name_ &&
         handle_to_key_map_ == other.handle_to_key_map_ &&
         key_directive_.has_value() == other.key_directive_.has_value() &&
         (!key_directive_ || key_directive_->SerializeAsString() ==
                                 other.key_directive_->SerializeAsString());
}

bool CryptAuthKeyBundle::operator!=(const CryptAuthKeyBundle& other) const {
  return !(*this == other);
}

}  // namespace ash::device_sync
