// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_CHALLENGE_RESPONSE_KNOWN_USER_PREF_UTILS_H_
#define CHROMEOS_LOGIN_AUTH_CHALLENGE_RESPONSE_KNOWN_USER_PREF_UTILS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "chromeos/login/auth/challenge_response_key.h"

namespace base {
class Value;
}  // namespace base

namespace chromeos {

// Builds the known_user value that contains information about the given
// challenge-response keys that can be used by the user to authenticate.
//
// The format currently is a list of dictionaries, each with the following keys:
// * "public_key_spki" - contains the base64-encoded DER blob of the X.509
//   Subject Public Key Info.
// * "extension_id" - contains the base64-encoded id of the extension that is
//   used to sign the key.
base::Value COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH)
    SerializeChallengeResponseKeysForKnownUser(
        const std::vector<ChallengeResponseKey>& challenge_response_keys);

bool COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH)
    DeserializeChallengeResponseKeyFromKnownUser(
        const base::Value& pref_value,
        std::vector<DeserializedChallengeResponseKey>*
            deserialized_challenge_response_keys);

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace ash {
using ::chromeos::SerializeChallengeResponseKeysForKnownUser;
}

#endif  // CHROMEOS_LOGIN_AUTH_CHALLENGE_RESPONSE_KNOWN_USER_PREF_UTILS_H_
