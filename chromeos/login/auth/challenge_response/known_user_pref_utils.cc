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

}  // namespace

base::Value SerializeChallengeResponseKeysForKnownUser(
    const std::vector<ChallengeResponseKey>& challenge_response_keys) {
  base::Value pref_value(base::Value::Type::LIST);
  for (const auto& key : challenge_response_keys) {
    std::string spki_base64;
    base::Base64Encode(key.public_key_spki_der(), &spki_base64);
    base::Value key_representation(base::Value::Type::DICTIONARY);
    key_representation.SetKey(kPublicKeySpkiKey, base::Value(spki_base64));
    pref_value.Append(std::move(key_representation));
  }
  return pref_value;
}

bool DeserializeChallengeResponseKeysFromKnownUser(
    const base::Value& pref_value,
    std::vector<std::string>* public_key_spki_list) {
  public_key_spki_list->clear();
  if (!pref_value.is_list())
    return false;
  for (const base::Value& key_representation : pref_value.GetList()) {
    if (!key_representation.is_dict()) {
      LOG(WARNING) << "Ignoring challenge-response key info: not a dictionary";
      continue;
    }
    const base::Value* spki_base64 = key_representation.FindKeyOfType(
        kPublicKeySpkiKey, base::Value::Type::STRING);
    if (!spki_base64) {
      LOG(WARNING)
          << "Ignoring challenge-response key info: missing SPKI property";
      continue;
    }
    std::string spki;
    if (!base::Base64Decode(spki_base64->GetString(), &spki)) {
      LOG(WARNING)
          << "Ignoring challenge-response key info: invalid SPKI base64";
      continue;
    }
    if (spki.empty()) {
      LOG(WARNING) << "Ignoring challenge-response key info: empty SPKI";
      continue;
    }
    public_key_spki_list->emplace_back(std::move(spki));
  }
  return !public_key_spki_list->empty();
}

}  // namespace chromeos
