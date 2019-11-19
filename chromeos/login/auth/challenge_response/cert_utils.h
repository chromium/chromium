// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_LOGIN_AUTH_CHALLENGE_RESPONSE_CERT_UTILS_H_
#define CHROMEOS_LOGIN_AUTH_CHALLENGE_RESPONSE_CERT_UTILS_H_

#include <cstdint>
#include <vector>

#include "base/component_export.h"
#include "base/optional.h"
#include "chromeos/login/auth/challenge_response_key.h"

namespace net {
class X509Certificate;
}  // namespace net

namespace chromeos {

// Maps from the TLS 1.3 SignatureScheme value into the challenge-response key
// algorithm.
COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH)
base::Optional<ChallengeResponseKey::SignatureAlgorithm>
GetChallengeResponseKeyAlgorithmFromSsl(uint16_t ssl_algorithm);

// Constructs the ChallengeResponseKey instance based on the public key referred
// by the specified certificate and on the specified list of supported
// algorithms. Returns false on failure.
bool COMPONENT_EXPORT(CHROMEOS_LOGIN_AUTH) ExtractChallengeResponseKeyFromCert(
    const net::X509Certificate& certificate,
    const std::vector<ChallengeResponseKey::SignatureAlgorithm>&
        signature_algorithms,
    ChallengeResponseKey* challenge_response_key);

}  // namespace chromeos

#endif  // CHROMEOS_LOGIN_AUTH_CHALLENGE_RESPONSE_CERT_UTILS_H_
