// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/memory/values_equivalent.h"
#include "base/notreached.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_util.h"
#include "chromeos/ash/components/dbus/cryptohome/key.pb.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/known_user.h"

namespace cryptohome {

namespace {

using ::ash::ChallengeResponseKey;

}  //  anonymous namespace

Identification::Identification() = default;

Identification::Identification(const AccountId& account_id)
    : id_(GetCryptohomeId(account_id)) {}

Identification::Identification(const std::string& id) : id_(id) {}

Identification Identification::FromString(const std::string& id) {
  return Identification(id);
}

bool Identification::operator==(const Identification& other) const {
  return id_ == other.id_;
}

bool Identification::operator<(const Identification& right) const {
  return id_ < right.id_;
}

AccountIdentifier CreateAccountIdentifierFromAccountId(const AccountId& id) {
  AccountIdentifier out;
  out.set_account_id(GetCryptohomeId(id));
  return out;
}

AccountIdentifier CreateAccountIdentifierFromIdentification(
    const Identification& id) {
  AccountIdentifier out;
  out.set_account_id(id.id());
  return out;
}

KeyDefinition::ProviderData::ProviderData() = default;

KeyDefinition::ProviderData::ProviderData(const std::string& name)
    : name(name) {}

KeyDefinition::ProviderData::ProviderData(const ProviderData& other)
    : name(other.name) {
  if (other.number) {
    number = std::make_unique<int64_t>(*other.number);
  }
  if (other.bytes) {
    bytes = std::make_unique<std::string>(*other.bytes);
  }
}

KeyDefinition::ProviderData::ProviderData(const std::string& name,
                                          int64_t number)
    : name(name), number(new int64_t(number)) {}

KeyDefinition::ProviderData::ProviderData(const std::string& name,
                                          const std::string& bytes)
    : name(name), bytes(new std::string(bytes)) {}

void KeyDefinition::ProviderData::operator=(const ProviderData& other) {
  name = other.name;
  number.reset(other.number ? new int64_t(*other.number) : nullptr);
  bytes.reset(other.bytes ? new std::string(*other.bytes) : nullptr);
}

KeyDefinition::ProviderData::~ProviderData() = default;

bool KeyDefinition::ProviderData::operator==(const ProviderData& other) const {
  return name == other.name && base::ValuesEquivalent(number, other.number) &&
         base::ValuesEquivalent(bytes, other.bytes);
}

bool KeyDefinition::Policy::operator==(const Policy& other) const {
  return low_entropy_credential == other.low_entropy_credential &&
         auth_locked == other.auth_locked;
}

bool KeyDefinition::Policy::operator!=(const Policy& other) const {
  return !(*this == other);
}

KeyDefinition KeyDefinition::CreateForPassword(
    const std::string& secret,
    const KeyLabel& label,
    int /*AuthKeyPrivileges*/ privileges) {
  KeyDefinition key_def;
  key_def.type = TYPE_PASSWORD;
  key_def.label = label;
  key_def.privileges = privileges;
  key_def.secret = secret;
  return key_def;
}

KeyDefinition KeyDefinition::CreateForChallengeResponse(
    const std::vector<ChallengeResponseKey>& challenge_response_keys,
    const KeyLabel& label,
    int /*AuthKeyPrivileges*/ privileges) {
  KeyDefinition key_def;
  key_def.type = TYPE_CHALLENGE_RESPONSE;
  key_def.label = label;
  key_def.privileges = privileges;
  key_def.challenge_response_keys = challenge_response_keys;
  return key_def;
}

KeyDefinition::KeyDefinition() = default;

KeyDefinition::KeyDefinition(const KeyDefinition& other) = default;

KeyDefinition::~KeyDefinition() = default;

bool KeyDefinition::operator==(const KeyDefinition& other) const {
  if (type != other.type || label != other.label ||
      privileges != other.privileges || policy != other.policy ||
      revision != other.revision ||
      challenge_response_keys != other.challenge_response_keys ||
      provider_data.size() != other.provider_data.size()) {
    return false;
  }

  for (size_t i = 0; i < provider_data.size(); ++i) {
    if (!(provider_data[i] == other.provider_data[i])) {
      return false;
    }
  }
  return true;
}

Authorization::Authorization(const std::string& key, const KeyLabel& label)
    : key(key), label(label) {}

Authorization::Authorization(const KeyDefinition& key_def)
    : key(key_def.secret), label(key_def.label) {}

bool Authorization::operator==(const Authorization& other) const {
  return key == other.key && label == other.label;
}

Authorization::~Authorization() = default;

}  // namespace cryptohome
