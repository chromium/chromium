// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/quick_pair/fast_pair_decryption.h"

#include <fuzzer/FuzzedDataProvider.h>
#include <stddef.h>
#include <stdint.h>
#include <array>

#include "ash/quick_pair/common/logging.h"
#include "ash/quick_pair/fast_pair_handshake/fast_pair_encryption.h"
#include "base/no_destructor.h"

namespace {

struct Environment {
  Environment() {
    // Disable noisy logging for fuzzing.
    logging::SetMinLogLevel(logging::LOGGING_FATAL);
  }

  ash::quick_pair::ScopedDisableLoggingForTesting disable_logging_;
};

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Enforce an exact input length so that we can AES encrypt the fuzzed input.
  // Prioritizes code exploration over testing every possible input.
  if (size != 2 * ash::quick_pair::fast_pair_decryption::kBlockByteSize)
    return 0;

  static base::NoDestructor<Environment> env;
  FuzzedDataProvider fuzzed_data(data, size);

  std::vector<uint8_t> aes_key_bytes = fuzzed_data.ConsumeBytes<uint8_t>(
      ash::quick_pair::fast_pair_decryption::kBlockByteSize);
  std::array<uint8_t, ash::quick_pair::fast_pair_decryption::kBlockByteSize>
      aes_key_arr;
  std::copy_n(aes_key_bytes.begin(),
              ash::quick_pair::fast_pair_decryption::kBlockByteSize,
              aes_key_arr.begin());

  std::vector<uint8_t> data_bytes = fuzzed_data.ConsumeBytes<uint8_t>(
      ash::quick_pair::fast_pair_decryption::kBlockByteSize);
  std::array<uint8_t, ash::quick_pair::fast_pair_decryption::kBlockByteSize>
      data_arr;
  std::copy_n(data_bytes.begin(),
              ash::quick_pair::fast_pair_decryption::kBlockByteSize,
              data_arr.begin());

  auto encrypted_arr = ash::quick_pair::fast_pair_encryption::EncryptBytes(
      aes_key_arr, data_arr);

  ash::quick_pair::fast_pair_decryption::ParseDecryptedResponse(aes_key_arr,
                                                                encrypted_arr);

  ash::quick_pair::fast_pair_decryption::ParseDecryptedPasskey(aes_key_arr,
                                                               encrypted_arr);

  return 0;
}