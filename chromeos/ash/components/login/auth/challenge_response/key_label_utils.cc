// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/challenge_response/key_label_utils.h"

#include "base/check_op.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/sha2.h"

namespace ash {

namespace {

// Prefix to be used in the cryptohome key labels for challenge-response keys.
// Prefixing allows to avoid accidental clashes between different keys (labels
// are used to uniquely identify the needed key in requests to cryptohomed).
constexpr char kKeyLabelPrefix[] = "challenge-response-";

// Given a DER-encoded X.509 Subject Public Key Info, returns its hashed
// representation that is suitable for inclusion into cryptohome key labels. The
// label built this way is, practically, unique (short of a SHA-256 collision).
std::string GenerateLabelSpkiPart(const std::string& public_key_spki_der) {
  DCHECK(!public_key_spki_der.empty());
  return base::HexEncode(crypto::SHA256HashString(public_key_spki_der));
}

}  // namespace

std::string GenerateChallengeResponseKeyLabel(
    const std::vector<ChallengeResponseKey>& challenge_response_keys) {
  // For now, only a single challenge-response key is supported.
  DCHECK_EQ(challenge_response_keys.size(), 1ull);
  return kKeyLabelPrefix +
         GenerateLabelSpkiPart(
             challenge_response_keys[0].public_key_spki_der());
}

}  // namespace ash
