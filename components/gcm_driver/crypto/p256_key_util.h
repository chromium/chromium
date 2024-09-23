// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GCM_DRIVER_CRYPTO_P256_KEY_UTIL_H_
#define COMPONENTS_GCM_DRIVER_CRYPTO_P256_KEY_UTIL_H_

#include <string>
#include <string_view>

namespace crypto {
class ECPrivateKey;
}

namespace gcm {

// Writes the public key associated with the |key| to |*public_key| in
// uncompressed point format. That is, a 65-octet sequence that starts with a
// 0x04 octet. Returns whether the public key could be extracted successfully.
[[nodiscard]] bool GetRawPublicKey(const crypto::ECPrivateKey& key,
                                   std::string* public_key);

// Writes the private key associated with the |key| to |*private_key| as a PKCS
// #8 PrivateKeyInfo block. Returns whether the private key could be extracted
// successfully.
[[nodiscard]] bool GetRawPrivateKey(const crypto::ECPrivateKey& key,
                                    std::string* private_key);

// Computes the shared secret between |key| and |peer_public_key|.The
// |peer_public_key| must be an octet string in uncompressed form per
// SEC1 2.3.3.
//
// Returns whether the secret could be computed, and was written to the out
// argument.
[[nodiscard]] bool ComputeSharedP256Secret(crypto::ECPrivateKey& key,
                                           std::string_view peer_public_key,
                                           std::string* out_shared_secret);

}  // namespace gcm

#endif  // COMPONENTS_GCM_DRIVER_CRYPTO_P256_KEY_UTIL_H_
