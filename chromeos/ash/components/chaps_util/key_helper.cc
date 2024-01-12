// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/chaps_util/key_helper.h"

#include <pk11pub.h>
#include <stdint.h>

#include <vector>

#include "base/hash/sha1.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace chromeos {

crypto::ScopedSECItem MakeIdFromPubKeyNss(
    const std::vector<uint8_t>& public_key_bytes) {
  SECItem secitem_modulus;
  secitem_modulus.data = const_cast<uint8_t*>(public_key_bytes.data());
  secitem_modulus.len = public_key_bytes.size();
  return crypto::ScopedSECItem(PK11_MakeIDFromPubKey(&secitem_modulus));
}

std::vector<uint8_t> SECItemToBytes(const crypto::ScopedSECItem& id) {
  if (!id || id->len == 0) {
    return {};
  }
  return std::vector<uint8_t>(id->data, id->data + id->len);
}

std::vector<uint8_t> MakePkcs11IdForEcKey(base::span<const uint8_t> key_data) {
  if (key_data.size() <= base::kSHA1Length) {
    return std::vector<uint8_t>(key_data.begin(), key_data.end());
  }

  base::SHA1Digest hash = base::SHA1HashSpan(key_data);
  return std::vector<uint8_t>(hash.begin(), hash.end());
}

std::vector<uint8_t> GetEcPublicKeyBytes(const EC_KEY* ec_key) {
  if (!ec_key) {
    return {};
  }
  const EC_POINT* point = EC_KEY_get0_public_key(ec_key);
  const EC_GROUP* group = EC_KEY_get0_group(ec_key);

  if (!point || !group) {
    return {};
  }

  size_t point_len =
      EC_POINT_point2oct(group, point, POINT_CONVERSION_UNCOMPRESSED,
                         /*buf=*/nullptr, /*max_out=*/0, /*ctx=*/nullptr);
  if (point_len == 0) {
    return {};
  }

  std::vector<uint8_t> buf(point_len);
  if (EC_POINT_point2oct(group, point, POINT_CONVERSION_UNCOMPRESSED,
                         buf.data(), buf.size(),
                         /*ctx=*/nullptr) != buf.size()) {
    return {};
  }
  return buf;
}

std::vector<uint8_t> GetEcPrivateKeyBytes(const EC_KEY* ec_key) {
  if (!ec_key) {
    return {};
  }
  const EC_GROUP* group = EC_KEY_get0_group(ec_key);
  const BIGNUM* priv_key = EC_KEY_get0_private_key(ec_key);
  if (!priv_key || !group) {
    return {};
  }
  size_t priv_key_size_bits = EC_GROUP_order_bits(group);
  size_t priv_key_bytes = (priv_key_size_bits + 7) / 8;
  std::vector<uint8_t> buffer(priv_key_bytes);
  int extract_result =
      BN_bn2bin_padded(buffer.data(), priv_key_bytes, priv_key);

  if (!extract_result) {
    return {};
  }
  return buffer;
}

bool IsKeyEcType(const bssl::UniquePtr<EVP_PKEY>& key) {
  return EVP_PKEY_base_id(key.get()) == EVP_PKEY_EC;
}

bool IsKeyRsaType(const bssl::UniquePtr<EVP_PKEY>& key) {
  return EVP_PKEY_base_id(key.get()) == EVP_PKEY_RSA;
}

}  // namespace chromeos
