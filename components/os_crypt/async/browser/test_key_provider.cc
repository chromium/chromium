// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/async/browser/test_key_provider.h"

#include <array>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/span.h"
#include "components/os_crypt/async/common/algorithm.mojom.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "crypto/kdf.h"

namespace os_crypt_async {

TestKeyProvider::TestKeyProvider(const std::string& name,
                                 bool use_for_encryption,
                                 std::optional<Encryptor::Key> key)
    : name_(name),
      use_for_encryption_(use_for_encryption),
      key_(key.has_value()
               ? std::move(key.value())
               : Encryptor::Key(
                     crypto::kdf::Hkdf<Encryptor::Key::kAES256GCMKeySize>(
                         /*kind=*/crypto::hash::kSha256,
                         /*secret=*/base::as_byte_span(name_),
                         /*salt=*/std::to_array<uint8_t>({'s', 'a', 'l', 't'}),
                         /*info=*/
                         std::to_array<uint8_t>({'i', 'n', 'f', 'o'})),
                     mojom::Algorithm::kAES256GCM)) {}

TestKeyProvider::~TestKeyProvider() = default;

void TestKeyProvider::GetKey(KeyCallback callback) {
  std::move(callback).Run(name_, key_.Clone());
}

bool TestKeyProvider::UseForEncryption() {
  return use_for_encryption_;
}

}  // namespace os_crypt_async
