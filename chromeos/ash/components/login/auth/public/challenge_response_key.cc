// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"

namespace ash {

ChallengeResponseKey::ChallengeResponseKey() = default;

ChallengeResponseKey::ChallengeResponseKey(const ChallengeResponseKey& other) =
    default;

ChallengeResponseKey::~ChallengeResponseKey() = default;

bool ChallengeResponseKey::operator==(const ChallengeResponseKey& other) const {
  return public_key_spki_der_ == other.public_key_spki_der_ &&
         signature_algorithms_ == other.signature_algorithms_;
}

bool ChallengeResponseKey::operator!=(const ChallengeResponseKey& other) const {
  return !(*this == other);
}

}  // namespace ash
