// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/android/attestation_utils.h"

#include <string>

#include "base/base64.h"
#include "crypto/hash.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise {

class AttestationUtilsTest : public testing::Test {};

TEST_F(AttestationUtilsTest, CreateAttestationHashes_StandardInput) {
  std::string request_payload = "sample_request_payload";
  std::string timestamp = "1713895415";
  std::string nonce = "sample_nonce_123";

  AttestationHashes hashes =
      CreateAttestationHashes(request_payload, timestamp, nonce);

  EXPECT_EQ(hashes.request_hash, "ponewoeVrcQ3KXy4N+TJNwdrNoNO8RJiZe1eg8HHz6g=");
  EXPECT_EQ(hashes.timestamp_hash, "shloxL+uLwC7hyH5SxqtLSP0JylONKpUn8LJWo56XeU=");
  EXPECT_EQ(hashes.nonce_hash, "d0BuuvYH/ZKrZjKNjbyyTGWEblfTCur5oGz68kquD4o=");
}

TEST_F(AttestationUtilsTest, CreateAttestationHashes_EmptyInputs) {
  AttestationHashes hashes = CreateAttestationHashes("", "", "");

  EXPECT_EQ(hashes.request_hash, "XsH35wDzfD0LKYHQSFX8NLlKqhVFewXKVxgXRC0ij4E=");
  EXPECT_EQ(hashes.timestamp_hash, "47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU=");
  EXPECT_EQ(hashes.nonce_hash, "47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU=");
}

TEST_F(AttestationUtilsTest, CreateAttestationHashes_PartialEmptyInputs) {
  AttestationHashes hashes = CreateAttestationHashes("payload_only", "", "");

  EXPECT_EQ(hashes.request_hash, "IMNdJ0rvuH/3NgQfvQ2AF7jYn/hj8rlyHNIXHUhIULo=");
  EXPECT_EQ(hashes.timestamp_hash, "47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU=");
  EXPECT_EQ(hashes.nonce_hash, "47DEQpj8HBSa+/TImW+5JCeuQeRkm5NMpJWZG3hSuFU=");
}

}  // namespace enterprise
