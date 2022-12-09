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

constexpr std::array<uint8_t, 32> kEd25519PublicKey2 = {
    2, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 8, 0,
    0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 0, 0, 0, 0, 0, 0};

constexpr std::array<uint8_t, 64> kEd25519Signature = {
    0, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

constexpr std::array<uint8_t, 64> kEd25519Signature2 = {
    2, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

TEST(SignedWebBundleSignatureStackEntryTest, Getters) {
  SignedWebBundleSignatureStackEntry signature_stack_entry(
      /*complete_entry_cbor=*/{1, 2}, /*attributes_cbor=*/{3, 4, 5},
      Ed25519PublicKey::Create(base::make_span(kEd25519PublicKey)),
      Ed25519Signature::Create(base::make_span(kEd25519Signature)));

  EXPECT_THAT(signature_stack_entry.complete_entry_cbor(), ElementsAre(1, 2));
  EXPECT_THAT(signature_stack_entry.attributes_cbor(), ElementsAre(3, 4, 5));
  EXPECT_THAT(signature_stack_entry.public_key().bytes(),
              Eq(kEd25519PublicKey));
  EXPECT_THAT(signature_stack_entry.signature().bytes(), Eq(kEd25519Signature));
}

TEST(SignedWebBundleSignatureStackEntryTest, Equality) {
  SignedWebBundleSignatureStackEntry signature_stack_entry1a(
      /*complete_entry_cbor=*/{1, 2}, /*attributes_cbor=*/{3, 4, 5},
      Ed25519PublicKey::Create(base::make_span(kEd25519PublicKey)),
      Ed25519Signature::Create(base::make_span(kEd25519Signature)));

  SignedWebBundleSignatureStackEntry signature_stack_entry1b(
      /*complete_entry_cbor=*/{1, 2}, /*attributes_cbor=*/{3, 4, 5},
      Ed25519PublicKey::Create(base::make_span(kEd25519PublicKey)),
      Ed25519Signature::Create(base::make_span(kEd25519Signature)));

  SignedWebBundleSignatureStackEntry signature_stack_entry2(
      /*complete_entry_cbor=*/{1}, /*attributes_cbor=*/{3, 4, 5},
      Ed25519PublicKey::Create(base::make_span(kEd25519PublicKey)),
      Ed25519Signature::Create(base::make_span(kEd25519Signature)));

  SignedWebBundleSignatureStackEntry signature_stack_entry3(
      /*complete_entry_cbor=*/{1, 2}, /*attributes_cbor=*/{3},
      Ed25519PublicKey::Create(base::make_span(kEd25519PublicKey)),
      Ed25519Signature::Create(base::make_span(kEd25519Signature)));

  SignedWebBundleSignatureStackEntry signature_stack_entry4(
      /*complete_entry_cbor=*/{1, 2}, /*attributes_cbor=*/{3, 4, 5},
      Ed25519PublicKey::Create(base::make_span(kEd25519PublicKey2)),
      Ed25519Signature::Create(base::make_span(kEd25519Signature)));

  SignedWebBundleSignatureStackEntry signature_stack_entry5(
      /*complete_entry_cbor=*/{1, 2}, /*attributes_cbor=*/{3, 4, 5},
      Ed25519PublicKey::Create(base::make_span(kEd25519PublicKey)),
      Ed25519Signature::Create(base::make_span(kEd25519Signature2)));

  EXPECT_TRUE(signature_stack_entry1a == signature_stack_entry1a);
  EXPECT_TRUE(signature_stack_entry1a == signature_stack_entry1b);
  EXPECT_FALSE(signature_stack_entry1a != signature_stack_entry1a);
  EXPECT_FALSE(signature_stack_entry1a != signature_stack_entry1b);

  EXPECT_TRUE(signature_stack_entry1a != signature_stack_entry2);
  EXPECT_TRUE(signature_stack_entry1a != signature_stack_entry3);
  EXPECT_TRUE(signature_stack_entry1a != signature_stack_entry4);
  EXPECT_TRUE(signature_stack_entry1a != signature_stack_entry5);
  EXPECT_FALSE(signature_stack_entry1a == signature_stack_entry2);
  EXPECT_FALSE(signature_stack_entry1a == signature_stack_entry3);
  EXPECT_FALSE(signature_stack_entry1a == signature_stack_entry4);
  EXPECT_FALSE(signature_stack_entry1a == signature_stack_entry5);
}

}  // namespace

}  // namespace web_package
