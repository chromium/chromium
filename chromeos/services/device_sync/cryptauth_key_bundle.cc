// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key_bundle.h"

#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/values.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_constants.h"
#include "chromeos/services/device_sync/value_string_encoding.h"

namespace chromeos {

namespace device_sync {

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
                 CryptAuthKeyBundle::Name::kLegacyMasterKey,
                 CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether,
                 CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey});
  return *name_list;
}

const base::flat_set<CryptAuthKeyBundle::Name>&
CryptAuthKeyBundle::AllEnrollableNames() {
  static const base::NoDestructor<base::flat_set<CryptAuthKeyBundle::Name>>
      name_list({CryptAuthKeyBundle::Name::kUserKeyPair,
                 CryptAuthKeyBundle::Name::kLegacyMasterKey,
                 CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether});
  return *name_list;
}

// static
std::string CryptAuthKeyBundle::KeyBundleNameEnumToString(
    CryptAuthKeyBundle::Name name) {
  switch (name) {
    case CryptAuthKeyBundle::Name::kUserKeyPair:
      return kCryptAuthUserKeyPairName;
    case CryptAuthKeyBundle::Name::kLegacyMasterKey:
      return kCryptAuthLegacyMasterKeyName;
    case CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether:
      return kCryptAuthDeviceSyncBetterTogetherKeyName;
    case CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey:
      return kDeviceSyncBetterTogetherGroupKeyName;
  }
}

// static
base::Optional<CryptAuthKeyBundle::Name>
CryptAuthKeyBundle::KeyBundleNameStringToEnum(const std::string& name) {
  if (name == kCryptAuthUserKeyPairName)
    return CryptAuthKeyBundle::Name::kUserKeyPair;
  if (name == kCryptAuthLegacyMasterKeyName)
    return CryptAuthKeyBundle::Name::kLegacyMasterKey;
  if (name == kCryptAuthDeviceSyncBetterTogetherKeyName)
    return CryptAuthKeyBundle::Name::kDeviceSyncBetterTogether;
  if (name == kDeviceSyncBetterTogetherGroupKeyName)
    return CryptAuthKeyBundle::Name::kDeviceSyncBetterTogetherGroupKey;

  return base::nullopt;
}

// static
base::Optional<CryptAuthKeyBundle> CryptAuthKeyBundle::FromDictionary(
    const base::Value& dict) {
  if (!dict.is_dict())
    return base::nullopt;

  const std::string* name_string = dict.FindStringKey(kBundleNameDictKey);
  if (!name_string)
    return base::nullopt;

  base::Optional<CryptAuthKeyBundle::Name> name =
      KeyBundleNameStringToEnum(*name_string);
  if (!name)
    return base::nullopt;

  CryptAuthKeyBundle bundle(*name);

  const base::Value* keys = dict.FindKey(kKeyListDictKey);
  if (!keys || !keys->is_list())
    return base::nullopt;

  bool active_key_exists = false;
  for (const base::Value& key_dict : keys->GetList()) {
    base::Optional<CryptAuthKey> key = CryptAuthKey::FromDictionary(key_dict);
    if (!key)
      return base::nullopt;

    // Return nullopt if there are multiple active keys.
    if (key->status() == CryptAuthKey::Status::kActive) {
      if (active_key_exists)
        return base::nullopt;

      active_key_exists = true;
    }

    // Return nullopt if duplicate handles exist.
    if (base::Contains(bundle.handle_to_key_map(), key->handle()))
      return base::nullopt;

    bundle.AddKey(*key);
  }

  const base::Value* encoded_serialized_key_directive =
      dict.FindKey(kKeyDirectiveDictKey);
  if (encoded_serialized_key_directive) {
    base::Optional<cryptauthv2::KeyDirective> key_directive =
        util::DecodeProtoMessageFromValueString<cryptauthv2::KeyDirective>(
            encoded_serialized_key_directive);
    if (!key_directive)
      return base::nullopt;

    bundle.set_key_directive(*key_directive);
  }

  return bundle;
}

CryptAuthKeyBundle::CryptAuthKeyBundle(Name name) : name_(name) {}

CryptAuthKeyBundle::CryptAuthKeyBundle(const CryptAuthKeyBundle&) = default;

CryptAuthKeyBundle::~CryptAuthKeyBundle() = default;

const CryptAuthKey* CryptAuthKeyBundle::GetActiveKey() const {
  const auto& it = std::find_if(
      handle_to_key_map_.begin(), handle_to_key_map_.end(),
      [](const std::pair<std::string, CryptAuthKey>& handle_key_pair) -> bool {
        return handle_key_pair.second.status() == CryptAuthKey::Status::kActive;
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

base::Value CryptAuthKeyBundle::AsDictionary() const {
  base::Value dict(base::Value::Type::DICTIONARY);

  dict.SetKey(kBundleNameDictKey,
              base::Value(KeyBundleNameEnumToString(name_)));

  std::vector<base::Value> keys;
  std::transform(
      handle_to_key_map_.begin(), handle_to_key_map_.end(),
      std::back_inserter(keys),
      [](const std::pair<std::string, CryptAuthKey>& handle_key_pair) {
        if (handle_key_pair.second.IsSymmetricKey())
          return handle_key_pair.second.AsSymmetricKeyDictionary();

        DCHECK(handle_key_pair.second.IsAsymmetricKey());
        return handle_key_pair.second.AsAsymmetricKeyDictionary();
      });
  dict.SetKey(kKeyListDictKey, base::Value(keys));

  if (key_directive_) {
    dict.SetKey(kKeyDirectiveDictKey,
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

}  // namespace device_sync

}  // namespace chromeos
