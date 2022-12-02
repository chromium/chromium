// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"

#include <utility>

#include "base/containers/span.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
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
    base::span<const uint8_t, 32> public_key,
    base::span<const uint8_t, 64> signature) {
  auto raw_signature_stack_entry =
      mojom::BundleIntegrityBlockSignatureStackEntry::New();
  raw_signature_stack_entry->public_key = Ed25519PublicKey::Create(public_key);
  raw_signature_stack_entry->signature = Ed25519Signature::Create(signature);
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

}  // namespace web_package
