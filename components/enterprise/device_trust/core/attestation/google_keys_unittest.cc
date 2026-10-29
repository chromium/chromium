// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_trust/core/attestation/google_keys.h"

#include <array>
#include <cstdint>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

// Expected 5-byte key IDs including the leading 0x00 byte.
constexpr auto kExpectedDefaultVAEncryptionKeyID =
    std::to_array<uint8_t>({0x00, 0x4a, 0xe2, 0xdc, 0xae});
constexpr auto kExpectedTestVAEncryptionKeyID =
    std::to_array<uint8_t>({0x00, 0xef, 0x22, 0x0f, 0xb0});

}  // namespace

TEST(GoogleKeysTest, VAEncryptionKeyIDs) {
  GoogleKeys google_keys;

  EXPECT_EQ(
      base::as_byte_span(google_keys.va_encryption_key(DEFAULT_VA).key_id()),
      base::span(kExpectedDefaultVAEncryptionKeyID));
  EXPECT_EQ(base::as_byte_span(google_keys.va_encryption_key(TEST_VA).key_id()),
            base::span(kExpectedTestVAEncryptionKeyID));
}

TEST(GoogleKeysTest, VASigningKeysHaveNoKeyID) {
  GoogleKeys google_keys;

  EXPECT_FALSE(google_keys.va_signing_key(DEFAULT_VA).has_key_id());
  EXPECT_FALSE(google_keys.va_signing_key(TEST_VA).has_key_id());
}

}  // namespace enterprise_connectors
