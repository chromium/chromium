// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/fallback_linux_key_provider.h"

#include <utility>

#include "components/os_crypt/async/common/algorithm.mojom.h"

namespace os_crypt_async {

namespace {

// These constants are duplicated from the sync backend.
constexpr char kEncryptionTag[] = "v10";

// PBKDF2-HMAC-SHA1(1 iteration, key = "peanuts", salt = "saltysalt")
constexpr auto kV10Key =
    std::to_array<uint8_t>({0xfd, 0x62, 0x1f, 0xe5, 0xa2, 0xb4, 0x02, 0x53,
                            0x9d, 0xfa, 0x14, 0x7c, 0xa9, 0x27, 0x27, 0x78});

}  // namespace

FallbackLinuxKeyProvider::FallbackLinuxKeyProvider(bool use_for_encryption)
    : use_for_encryption_(use_for_encryption) {}

FallbackLinuxKeyProvider::~FallbackLinuxKeyProvider() = default;

void FallbackLinuxKeyProvider::GetKey(KeyCallback callback) {
  Encryptor::Key key(kV10Key, mojom::Algorithm::kAES128CBC);
  std::move(callback).Run(kEncryptionTag, std::move(key));
}

bool FallbackLinuxKeyProvider::UseForEncryption() {
  return use_for_encryption_;
}

bool FallbackLinuxKeyProvider::IsCompatibleWithOsCryptSync() {
  return false;
}

}  // namespace os_crypt_async
