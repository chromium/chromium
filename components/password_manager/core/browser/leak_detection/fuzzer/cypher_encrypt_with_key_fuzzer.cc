// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/strings/string_view_util.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "testing/libfuzzer/libfuzzer_base_wrappers.h"
#include "third_party/boringssl/src/include/openssl/nid.h"
#include "third_party/private-join-and-compute/src/private_join_and_compute/crypto/ec_commutative_cipher.h"

namespace password_manager {

DEFINE_LLVM_FUZZER_TEST_ONE_INPUT_SPAN(base::span<const uint8_t> data) {
  using ::private_join_and_compute::ECCommutativeCipher;

  if (data.empty()) {
    return 0;
  }
  uint8_t key_size = data[0];
  std::string_view input = base::as_string_view(data.subspan<1>());
  if (input.size() < key_size) {
    return 0;
  }

  std::string_view key = input.substr(0, key_size);
  // Check the key correctness. Otherwise, a crash happens.
  auto cipher = ECCommutativeCipher::CreateFromKey(NID_X9_62_prime256v1, key,
                                                   ECCommutativeCipher::SHA256);
  if (!cipher.ok()) {
    return 0;
  }

  std::string payload(input.substr(key_size));
  std::optional<std::string> result =
      CipherEncryptWithKey(payload, std::string(key));
  return 0;
}

}  // namespace password_manager
