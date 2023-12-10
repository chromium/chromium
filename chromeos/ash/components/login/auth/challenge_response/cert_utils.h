// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_CHALLENGE_RESPONSE_CERT_UTILS_H_
#define CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_CHALLENGE_RESPONSE_CERT_UTILS_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/component_export.h"
#include "chromeos/ash/components/login/auth/public/challenge_response_key.h"

namespace net {
class X509Certificate;
}  // namespace net

namespace ash {

// Maps from the TLS 1.3 SignatureScheme value into the challenge-response key
// algorithm.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
std::optional<ChallengeResponseKey::SignatureAlgorithm>
GetChallengeResponseKeyAlgorithmFromSsl(uint16_t ssl_algorithm);

// Constructs the ChallengeResponseKey instance based on the public key referred
// by the specified certificate and on the specified list of supported
// algorithms. Returns false on failure.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH)
bool ExtractChallengeResponseKeyFromCert(
    const net::X509Certificate& certificate,
    const std::vector<ChallengeResponseKey::SignatureAlgorithm>&
        signature_algorithms,
    ChallengeResponseKey* challenge_response_key);

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_LOGIN_AUTH_CHALLENGE_RESPONSE_CERT_UTILS_H_
