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
#include "crypto/ec_private_key.h"
#include "third_party/boringssl/src/include/openssl/ec.h"
#include "third_party/boringssl/src/include/openssl/ecdh.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace gcm {

namespace {

// A P-256 field element consists of 32 bytes.
const size_t kFieldBytes = 32;

// A P-256 point in uncompressed form consists of 0x04 (to denote that the point
// is uncompressed per SEC1 2.3.3) followed by two, 32-byte field elements.
const size_t kUncompressedPointBytes = 1 + 2 * kFieldBytes;

}  // namespace

bool GetRawPublicKey(const crypto::ECPrivateKey& key, std::string* public_key) {
  DCHECK(public_key);
  std::string candidate_public_key;

  // ECPrivateKey::ExportRawPublicKey() returns the EC point in the uncompressed
  // point format.
  if (!key.ExportRawPublicKey(&candidate_public_key) ||
      candidate_public_key.size() != kUncompressedPointBytes) {
    DLOG(ERROR) << "Unable to export the public key.";
    return false;
  }
  public_key->erase();
  public_key->reserve(kUncompressedPointBytes);
  public_key->append(candidate_public_key);
  return true;
}

// TODO(peter): Get rid of this once all key management code has been updated
// to use ECPrivateKey instead of std::string.
bool GetRawPrivateKey(const crypto::ECPrivateKey& key,
                      std::string* private_key) {
  DCHECK(private_key);
  std::vector<uint8_t> private_key_vector;
  if (!key.ExportPrivateKey(&private_key_vector))
    return false;
  private_key->assign(private_key_vector.begin(), private_key_vector.end());
  return true;
}

bool ComputeSharedP256Secret(crypto::ECPrivateKey& key,
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
