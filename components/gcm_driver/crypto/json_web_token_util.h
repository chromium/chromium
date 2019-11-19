// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_JSON_WEB_TOKEN_UTIL_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_JSON_WEB_TOKEN_UTIL_H_

#include <string>

#include "base/optional.h"
#include "base/values.h"

namespace crypto {
class ECPrivateKey;
}

namespace gcm {

// Creates JSON web token with provided |payload|, and sign  with provided
// |private_key|, as per RFC7519.
// |claims|: A Value of DICTIONARY type containing claims between two parties.
// |private_key|: An elliptic curve (EC) private key.
// Note: Currently only ES256 is supported, as ECPrivateKey only supports
// NIST P-256 curve and ECSignatureCreator is hardcoded to SHA256.
//
// https://tools.ietf.org/html/rfc7519
base::Optional<std::string> CreateJSONWebToken(
    const base::Value& claims,
    crypto::ECPrivateKey* private_key);

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_JSON_WEB_TOKEN_UTIL_H_
