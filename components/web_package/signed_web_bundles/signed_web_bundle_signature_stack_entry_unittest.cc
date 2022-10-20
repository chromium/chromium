// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"

#include <utility>

#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

constexpr uint8_t kEd25519PublicKey[32] = {0, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0,
                                           0, 8, 0, 8, 0, 0, 0, 0, 0, 0, 0,
                                           0, 8, 8, 8, 0, 0, 0, 0, 0, 0};

constexpr uint8_t kEd25519Signature[64] = {
    0, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

mojom::BundleIntegrityBlockSignatureStackEntryPtr MakeSignatureStackEntry(
    base::span<const uint8_t> public_key,
    base::span<const uint8_t> signature) {
  auto raw_signature_stack_entry =
      mojom::BundleIntegrityBlockSignatureStackEntry::New();
  raw_signature_stack_entry->public_key =
      std::vector(std::begin(public_key), std::end(public_key));
  raw_signature_stack_entry->signature =
      std::vector(std::begin(signature), std::end(signature));
  return raw_signature_stack_entry;
}

}  // namespace

TEST(SignedWebBundleSignatureStackEntryTest, TestValidSignatureStackEntry) {
  auto signature_stack_entry = SignedWebBundleSignatureStackEntry::Create(
      MakeSignatureStackEntry(kEd25519PublicKey, kEd25519Signature));

  ASSERT_TRUE(signature_stack_entry.has_value())
      << signature_stack_entry.error();
  EXPECT_TRUE(base::ranges::equal(signature_stack_entry->public_key().bytes(),
                                  kEd25519PublicKey));
  EXPECT_TRUE(base::ranges::equal(signature_stack_entry->signature().bytes(),
                                  kEd25519Signature));
}

TEST(SignedWebBundleSignatureStackEntryTest, TestInvalidPublicKey) {
  auto signature_stack_entry = SignedWebBundleSignatureStackEntry::Create(
      MakeSignatureStackEntry({}, kEd25519Signature));

  ASSERT_FALSE(signature_stack_entry.has_value());
  EXPECT_EQ(signature_stack_entry.error(),
            "Invalid public key: The Ed25519 public key does not have the "
            "correct length. Expected 32 bytes, but received 0 bytes.");
}

TEST(SignedWebBundleSignatureStackEntryTest, TestInvalidSignature) {
  auto signature_stack_entry = SignedWebBundleSignatureStackEntry::Create(
      MakeSignatureStackEntry(kEd25519PublicKey, {}));

  ASSERT_FALSE(signature_stack_entry.has_value());
  EXPECT_EQ(signature_stack_entry.error(),
            "Invalid signature: The signature has the wrong length. Expected "
            "64, but got 0 bytes.");
}

}  // namespace web_package
