// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"

#include <array>

#include "base/containers/span.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

using testing::ElementsAre;
using testing::Eq;

constexpr std::array<uint8_t, 32> kEd25519PublicKey = {
    0, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 8, 0,
    0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 0, 0, 0, 0, 0, 0};

constexpr std::array<uint8_t, 64> kEd25519Signature = {
    0, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

TEST(SignedWebBundleSignatureStackEntryTest, Getters) {
  SignedWebBundleSignatureStackEntry signature_stack_entry(
      /*attributes_cbor=*/{3, 4, 5},
      SignedWebBundleSignatureInfoEd25519(
          Ed25519PublicKey::Create(base::make_span(kEd25519PublicKey)),
          Ed25519Signature::Create(base::make_span(kEd25519Signature))));

  EXPECT_THAT(signature_stack_entry.attributes_cbor(), ElementsAre(3, 4, 5));

  auto* ed25519_signature_info =
      absl::get_if<web_package::SignedWebBundleSignatureInfoEd25519>(
          &signature_stack_entry.signature_info());
  ASSERT_TRUE(ed25519_signature_info);
  EXPECT_THAT(ed25519_signature_info->public_key().bytes(),
              Eq(kEd25519PublicKey));
  EXPECT_THAT(ed25519_signature_info->signature().bytes(),
              Eq(kEd25519Signature));
}

}  // namespace

}  // namespace web_package
