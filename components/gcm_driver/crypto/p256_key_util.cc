// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/p256_key_util.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>
#include <vector>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "crypto/keypair.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace gcm {

namespace {

// A P-256 field element consists of 32 bytes.
const size_t kFieldBytes = 32;

}  // namespace

bool ComputeSharedP256Secret(crypto::keypair::PrivateKey key,
                             std::string_view peer_public_key,
                             std::string* out_shared_secret) {
  DCHECK(out_shared_secret);

  EC_KEY* ec_private_key = EVP_PKEY_get0_EC_KEY(key.key());
  if (!ec_private_key || !EC_KEY_check_key(ec_private_key)) {
    DLOG(ERROR) << "The private key is invalid.";
    return false;
  }

  bssl::UniquePtr<EC_POINT> point(
      EC_POINT_new(EC_KEY_get0_group(ec_private_key)));

  if (!point || !EC_POINT_oct2point(
                    EC_KEY_get0_group(ec_private_key), point.get(),
                    reinterpret_cast<const uint8_t*>(peer_public_key.data()),
                    peer_public_key.size(), nullptr)) {
    DLOG(ERROR) << "Can't convert peer public value to curve point.";
    return false;
  }

  uint8_t result[kFieldBytes];
  if (ECDH_compute_key(result, sizeof(result), point.get(), ec_private_key,
                       nullptr) != sizeof(result)) {
    DLOG(ERROR) << "Unable to compute the ECDH shared secret.";
    return false;
  }

  out_shared_secret->assign(reinterpret_cast<char*>(result), sizeof(result));
  return true;
}

}  // namespace gcm
