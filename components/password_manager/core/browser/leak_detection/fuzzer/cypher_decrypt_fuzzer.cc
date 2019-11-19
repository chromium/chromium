// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"

#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/private-join-and-compute/src/crypto/ec_commutative_cipher.h"

namespace password_manager {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  using ::private_join_and_compute::ECCommutativeCipher;

  if (size < 1)
    return 0;
  uint8_t key_size = data[0];
  data++;
  size--;

  if (size < key_size)
    return 0;

  std::string key(reinterpret_cast<const char*>(data), key_size);
  data += key_size;
  size -= key_size;

  // Check the key correctness. Otherwise, a crash happens.
  auto cipher = ECCommutativeCipher::CreateFromKey(NID_X9_62_prime256v1, key,
                                                   ECCommutativeCipher::SHA256);
  if (!cipher.ok())
    return 0;

  std::string payload(reinterpret_cast<const char*>(data), size);
  std::string result = password_manager::CipherDecrypt(payload, key);
  return 0;
}

}  // namespace password_manager
