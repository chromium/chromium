// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_CHALLENGE_RESPONSE_KEY_LABEL_UTILS_H_
#define CHROMEOS_LOGIN_AUTH_CHALLENGE_RESPONSE_KEY_LABEL_UTILS_H_

#include <string>
#include <vector>

#include "chromeos/login/auth/challenge_response_key.h"

namespace chromeos {

// Generates the cryptohome user key label for the given challenge-response key
// information. Currently the constraint is that |challenge_response_keys| must
// contain exactly one item.
std::string GenerateChallengeResponseKeyLabel(
    const std::vector<ChallengeResponseKey>& challenge_response_keys);

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_CHALLENGE_RESPONSE_KEY_LABEL_UTILS_H_
