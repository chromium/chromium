// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/challenge_response/known_user_pref_utils.h"

#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "base/values.h"

namespace chromeos {

namespace {

constexpr char kPublicKeySpkiKey[] = "public_key_spki";
constexpr char kExtensionId[] = "extension_id";

void DeserializeSpkiKey(const base::Value& key_representation,
                        std::string* spki_key) {
  const base::Value* spki_base64 = key_representation.FindKeyOfType(
      kPublicKeySpkiKey, base::Value::Type::STRING);
  if (!spki_base64) {
    LOG(WARNING)
        << "Ignoring challenge-response key info: missing SPKI property";
    return;
  }
  if (!base::Base64Decode(spki_base64->GetString(), spki_key)) {
    LOG(WARNING) << "Ignoring challenge-response key info: invalid SPKI base64";
    return;
  }
  if (spki_key->empty()) {
    LOG(WARNING) << "Ignoring challenge-response key info: empty SPKI";
  }
}

void DeserializeExtensionId(const base::Value& key_representation,
                            std::string* extension_id) {
  const base::Value* extension_id_value =
      key_representation.FindKeyOfType(kExtensionId, base::Value::Type::STRING);
  if (!extension_id_value) {
    LOG(WARNING)
        << "Missing extension id property in challenge-response key info.";
    return;
  }
  *extension_id = extension_id_value->GetString();
  if (extension_id->empty()) {
    LOG(WARNING) << "Empty extension id in challenge-response key info.";
  }
}

}  // namespace

base::Value SerializeChallengeResponseKeysForKnownUser(
    const std::vector<ChallengeResponseKey>& challenge_response_keys) {
  base::Value pref_value(base::Value::Type::LIST);
  for (const auto& key : challenge_response_keys) {
    std::string spki_base64;
    base::Base64Encode(key.public_key_spki_der(), &spki_base64);
    base::Value key_representation(base::Value::Type::DICTIONARY);
    key_representation.SetKey(kPublicKeySpkiKey, base::Value(spki_base64));
    key_representation.SetKey(kExtensionId, base::Value(key.extension_id()));
    pref_value.Append(std::move(key_representation));
  }
  return pref_value;
}

bool DeserializeChallengeResponseKeyFromKnownUser(
    const base::Value& pref_value,
    std::vector<DeserializedChallengeResponseKey>*
        deserialized_challenge_response_keys) {
  deserialized_challenge_response_keys->clear();
  if (!pref_value.is_list())
    return false;
  for (const base::Value& key_representation : pref_value.GetList()) {
    if (!key_representation.is_dict()) {
      LOG(WARNING) << "Ignoring challenge-response key info: not a dictionary";
      continue;
    }
    DeserializedChallengeResponseKey deserialized_challenge_response_key;
    DeserializeSpkiKey(
        key_representation,
        &deserialized_challenge_response_key.public_key_spki_der);
    DeserializeExtensionId(key_representation,
                           &deserialized_challenge_response_key.extension_id);
    if (!deserialized_challenge_response_key.public_key_spki_der.empty()) {
      deserialized_challenge_response_keys->push_back(
          deserialized_challenge_response_key);
    }
  }
  return !deserialized_challenge_response_keys->empty();
}

}  // namespace chromeos
