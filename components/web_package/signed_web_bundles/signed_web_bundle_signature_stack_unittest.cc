// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack.h"

#include <array>

#include "base/containers/span.h"
#include "base/test/gmock_expected_support.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_signature_stack_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

constexpr std::array<uint8_t, 32> kTestPublicKey1 = {
    0x01, 0x23, 0x43, 0x43, 0x33, 0x42, 0x7A, 0x14, 0x42, 0x14, 0xa2,
    0xb6, 0xc2, 0xd9, 0xf2, 0x02, 0x03, 0x42, 0x18, 0x10, 0x12, 0x26,
    0x62, 0x88, 0xf6, 0xa3, 0xa5, 0x47, 0x14, 0x69, 0x00, 0x73};

constexpr std::array<uint8_t, 32> kTestPublicKey2 = {
    222, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6,
    7,   8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2};

constexpr std::array<uint8_t, 64> kTestSignature1 = {
    111, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2,
    3,   4, 5, 6, 7, 8, 9, 0, 1, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2,
    3,   4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2};
constexpr std::array<uint8_t, 64> kTestSignature2 = {
    222, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2,
    3,   4, 5, 6, 7, 8, 9, 0, 1, 2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2,
    3,   4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2};

TEST(SignedWebBundleSignatureStack,
     CreateFromEmptyVectorOfSignedWebBundleSignatureStackEntry) {
  std::vector<SignedWebBundleSignatureStackEntry> entries;
  auto result = SignedWebBundleSignatureStack::Create(entries);
  EXPECT_THAT(result, base::test::ErrorIs(
                          "The signature stack needs at least one entry."));
}

TEST(SignedWebBundleSignatureStack,
     CreateFromEmptyVectorOfBundleIntegrityBlockSignatureStackEntryPtr) {
  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr> entries;
  auto result = SignedWebBundleSignatureStack::Create(std::move(entries));
  EXPECT_THAT(result, base::test::ErrorIs(
                          "The signature stack needs at least one entry."));
}

TEST(SignedWebBundleSignatureStack,
     CreateFromVectorOfSignedWebBundleSignatureStackEntry) {
  SignedWebBundleSignatureStackEntry entry(
      /*attributes_cbor=*/{4, 5},
      SignedWebBundleSignatureInfoEd25519(
          Ed25519PublicKey::Create(base::make_span(kTestPublicKey1)),
          Ed25519Signature::Create(base::make_span(kTestSignature1))));

  std::vector<SignedWebBundleSignatureStackEntry> entries = {entry};
  ASSERT_OK_AND_ASSIGN(auto result,
                       SignedWebBundleSignatureStack::Create(entries));
  EXPECT_EQ(result.size(), 1u);
  EXPECT_EQ(result.entries()[0], entry);
}

TEST(SignedWebBundleSignatureStack,
     CreateFromVectorOfMultipleSignedWebBundleSignatureStackEntry) {
  SignedWebBundleSignatureStackEntry entry1(
      /*attributes_cbor=*/{4, 5},
      SignedWebBundleSignatureInfoEd25519(
          Ed25519PublicKey::Create(base::make_span(kTestPublicKey1)),
          Ed25519Signature::Create(base::make_span(kTestSignature1))));
  SignedWebBundleSignatureStackEntry entry2(
      /*attributes_cbor=*/{8, 9, 0},
      SignedWebBundleSignatureInfoEd25519(
          Ed25519PublicKey::Create(base::make_span(kTestPublicKey2)),
          Ed25519Signature::Create(base::make_span(kTestSignature2))));

  std::vector<SignedWebBundleSignatureStackEntry> entries = {entry1, entry2};
  ASSERT_OK_AND_ASSIGN(auto result,
                       SignedWebBundleSignatureStack::Create(entries));
  EXPECT_EQ(result.size(), 2u);
  EXPECT_EQ(result.entries()[0], entry1);
  EXPECT_EQ(result.entries()[1], entry2);
}

TEST(SignedWebBundleSignatureStack,
     CreateFromVectorOfBundleIntegrityBlockSignatureStackEntryPtr) {
  auto entry = mojom::BundleIntegrityBlockSignatureStackEntry::New();
  entry->attributes_cbor = {4, 5};

  auto ed25519_signature_info = mojom::SignatureInfoEd25519::New();
  ed25519_signature_info->public_key =
      Ed25519PublicKey::Create(base::make_span(kTestPublicKey1));
  ed25519_signature_info->signature =
      Ed25519Signature::Create(base::make_span(kTestSignature1));

  entry->signature_info =
      mojom::SignatureInfo::NewEd25519(std::move(ed25519_signature_info));

  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr> entries;
  entries.push_back(entry->Clone());
  ASSERT_OK_AND_ASSIGN(
      auto result, SignedWebBundleSignatureStack::Create(std::move(entries)));
  EXPECT_EQ(result.size(), 1u);

  auto* ed25519_signature_info_ptr =
      absl::get_if<web_package::SignedWebBundleSignatureInfoEd25519>(
          &result.entries()[0].signature_info());
  ASSERT_TRUE(ed25519_signature_info_ptr);

  EXPECT_EQ(result.entries()[0].attributes_cbor(), entry->attributes_cbor);
  EXPECT_EQ(ed25519_signature_info_ptr->public_key(),
            entry->signature_info->get_ed25519()->public_key);
  EXPECT_EQ(ed25519_signature_info_ptr->signature(),
            entry->signature_info->get_ed25519()->signature);
}

}  // namespace

}  // namespace web_package
