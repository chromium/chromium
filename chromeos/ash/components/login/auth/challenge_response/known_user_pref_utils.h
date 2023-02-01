// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_CHALLENGE_RESPONSE_KNOWN_USER_PREF_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_CHALLENGE_RESPONSE_KNOWN_USER_PREF_UTILS_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/values.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"

namespace ash {

// Builds the known_user value that contains information about the given
// challenge-response keys that can be used by the user to authenticate.
//
// The format currently is a list of dictionaries, each with the following keys:
// * "public_key_spki" - contains the base64-encoded DER blob of the X.509
//   Subject Public Key Info.
// * "extension_id" - contains the base64-encoded id of the extension that is
//   used to sign the key.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
base::Value::List SerializeChallengeResponseKeysForKnownUser(
    const std::vector<ChallengeResponseKey>& challenge_response_keys);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
bool DeserializeChallengeResponseKeyFromKnownUser(
    const base::Value::List& pref_value,
    std::vector<DeserializedChallengeResponseKey>*
        deserialized_challenge_response_keys);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_CHALLENGE_RESPONSE_KNOWN_USER_PREF_UTILS_H_
