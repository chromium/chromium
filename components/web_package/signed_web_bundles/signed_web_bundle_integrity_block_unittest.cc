// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"

#include <utility>

#include "base/containers/span.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/ed25519_public_key.h"
#include "components/web_package/signed_web_bundles/ed25519_signature.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

using testing::ElementsAreArray;

constexpr std::array<uint8_t, 32> kEd25519PublicKey1 = {
    0, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0, 0, 8, 0, 8, 0,
    0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 0, 0, 0, 0, 0, 0};

constexpr std::array<uint8_t, 32> kEd25519PublicKey2 = {
    0, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0, 0, 8, 0, 8, 8,
    0, 8, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8, 0, 0, 0};

constexpr std::array<uint8_t, 64> kEd25519Signature1 = {
    0, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

constexpr std::array<uint8_t, 64> kEd25519Signature2 = {
    0, 0, 0, 0, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

constexpr uint8_t kCompleteEntryCbor1[] = {'e', 1, 1, 1};
constexpr uint8_t kCompleteEntryCbor2[] = {'e', 2, 2, 2};

constexpr uint8_t kAttributesCbor1[] = {'a', 1, 1, 1};
constexpr uint8_t kAttributesCbor2[] = {'a', 2, 2, 2};

mojom::BundleIntegrityBlockSignatureStackEntryPtr MakeSignatureStackEntry(
    base::span<const uint8_t, 32> public_key,
    base::span<const uint8_t, 64> signature,
    base::span<const uint8_t> complete_entry_cbor,
    base::span<const uint8_t> attributes_cbor) {
  auto raw_signature_stack_entry =
      mojom::BundleIntegrityBlockSignatureStackEntry::New();
  raw_signature_stack_entry->public_key = Ed25519PublicKey::Create(public_key);
  raw_signature_stack_entry->signature = Ed25519Signature::Create(signature);
  raw_signature_stack_entry->complete_entry_cbor = std::vector(
      std::begin(complete_entry_cbor), std::end(complete_entry_cbor));
  raw_signature_stack_entry->attributes_cbor =
      std::vector(std::begin(attributes_cbor), std::end(attributes_cbor));
  return raw_signature_stack_entry;
}

}  // namespace

TEST(SignedWebBundleIntegrityBlockTest, InvalidSize) {
  auto raw_integrity_block = mojom::BundleIntegrityBlock::New();

  auto integrity_block =
      SignedWebBundleIntegrityBlock::Create(std::move(raw_integrity_block));
  ASSERT_FALSE(integrity_block.has_value());
  EXPECT_EQ(integrity_block.error(),
            "Cannot create integrity block with a size of 0.");
}

TEST(SignedWebBundleIntegrityBlockTest, EmptySignatureStack) {
  auto raw_integrity_block = mojom::BundleIntegrityBlock::New();
  raw_integrity_block->size = 42;

  auto integrity_block =
      SignedWebBundleIntegrityBlock::Create(std::move(raw_integrity_block));
  ASSERT_FALSE(integrity_block.has_value());
  EXPECT_EQ(integrity_block.error(),
            "Cannot create an integrity block: The signature stack needs at "
            "least one entry.");
}

TEST(SignedWebBundleIntegrityBlockTest, ValidIntegrityBlockWithOneSignature) {
  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>
      raw_signature_stack;
  raw_signature_stack.push_back(
      MakeSignatureStackEntry(kEd25519PublicKey1, kEd25519Signature1,
                              kCompleteEntryCbor1, kAttributesCbor1));

  auto raw_integrity_block = mojom::BundleIntegrityBlock::New();
  raw_integrity_block->size = 42;
  raw_integrity_block->signature_stack = std::move(raw_signature_stack);

  auto integrity_block =
      SignedWebBundleIntegrityBlock::Create(std::move(raw_integrity_block));
  ASSERT_TRUE(integrity_block.has_value());
  EXPECT_EQ(integrity_block->size_in_bytes(), 42ul);

  const auto& signature_stack = integrity_block->signature_stack();
  EXPECT_EQ(signature_stack.size(), 1ul);
  EXPECT_EQ(signature_stack.entries()[0].public_key().bytes(),
            kEd25519PublicKey1);
  EXPECT_EQ(signature_stack.entries()[0].signature().bytes(),
            kEd25519Signature1);
  EXPECT_THAT(signature_stack.entries()[0].complete_entry_cbor(),
              ElementsAreArray(kCompleteEntryCbor1));
  EXPECT_THAT(signature_stack.entries()[0].attributes_cbor(),
              ElementsAreArray(kAttributesCbor1));
}

TEST(SignedWebBundleIntegrityBlockTest, ValidIntegrityBlockWithTwoSignatures) {
  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>
      raw_signature_stack;
  raw_signature_stack.push_back(
      MakeSignatureStackEntry(kEd25519PublicKey1, kEd25519Signature1,
                              kCompleteEntryCbor1, kAttributesCbor1));
  raw_signature_stack.push_back(
      MakeSignatureStackEntry(kEd25519PublicKey2, kEd25519Signature2,
                              kCompleteEntryCbor2, kAttributesCbor2));

  auto raw_integrity_block = mojom::BundleIntegrityBlock::New();
  raw_integrity_block->size = 42;
  raw_integrity_block->signature_stack = std::move(raw_signature_stack);

  auto integrity_block =
      SignedWebBundleIntegrityBlock::Create(std::move(raw_integrity_block));
  ASSERT_TRUE(integrity_block.has_value());
  EXPECT_EQ(integrity_block->size_in_bytes(), 42ul);

  const auto& signature_stack = integrity_block->signature_stack();
  EXPECT_EQ(signature_stack.size(), 2ul);
  EXPECT_EQ(signature_stack.entries()[0].public_key().bytes(),
            kEd25519PublicKey1);
  EXPECT_EQ(signature_stack.entries()[0].signature().bytes(),
            kEd25519Signature1);
  EXPECT_THAT(signature_stack.entries()[0].complete_entry_cbor(),
              ElementsAreArray(kCompleteEntryCbor1));
  EXPECT_THAT(signature_stack.entries()[0].attributes_cbor(),
              ElementsAreArray(kAttributesCbor1));
  EXPECT_EQ(signature_stack.entries()[1].public_key().bytes(),
            kEd25519PublicKey2);
  EXPECT_EQ(signature_stack.entries()[1].signature().bytes(),
            kEd25519Signature2);
  EXPECT_THAT(signature_stack.entries()[1].complete_entry_cbor(),
              ElementsAreArray(kCompleteEntryCbor2));
  EXPECT_THAT(signature_stack.entries()[1].attributes_cbor(),
              ElementsAreArray(kAttributesCbor2));
}

TEST(SignedWebBundleIntegrityBlockTest, Comparators) {
  auto entry1 = MakeSignatureStackEntry(kEd25519PublicKey1, kEd25519Signature1,
                                        kCompleteEntryCbor1, kAttributesCbor1);
  auto entry2 = MakeSignatureStackEntry(kEd25519PublicKey2, kEd25519Signature2,
                                        kCompleteEntryCbor2, kAttributesCbor2);

  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>
      raw_signature_stack1;
  raw_signature_stack1.push_back(entry1->Clone());
  auto raw_integrity_block1 = mojom::BundleIntegrityBlock::New();
  raw_integrity_block1->size = 42;
  raw_integrity_block1->signature_stack = std::move(raw_signature_stack1);

  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>
      raw_signature_stack2;
  raw_signature_stack2.push_back(entry2->Clone());
  auto raw_integrity_block2 = mojom::BundleIntegrityBlock::New();
  raw_integrity_block2->size = 42;
  raw_integrity_block2->signature_stack = std::move(raw_signature_stack2);

  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>
      raw_signature_stack3;
  raw_signature_stack3.push_back(entry1->Clone());
  auto raw_integrity_block3 = mojom::BundleIntegrityBlock::New();
  raw_integrity_block3->size = 9999999;
  raw_integrity_block3->signature_stack = std::move(raw_signature_stack3);

  SignedWebBundleIntegrityBlock block1a =
      *SignedWebBundleIntegrityBlock::Create(raw_integrity_block1->Clone());
  SignedWebBundleIntegrityBlock block1b =
      *SignedWebBundleIntegrityBlock::Create(raw_integrity_block1->Clone());

  SignedWebBundleIntegrityBlock block2 =
      *SignedWebBundleIntegrityBlock::Create(raw_integrity_block2->Clone());

  SignedWebBundleIntegrityBlock block3 =
      *SignedWebBundleIntegrityBlock::Create(raw_integrity_block3->Clone());

  EXPECT_TRUE(block1a == block1a);
  EXPECT_TRUE(block1a == block1b);
  EXPECT_FALSE(block1a == block2);
  EXPECT_FALSE(block2 == block3);
  EXPECT_FALSE(block1a == block3);

  EXPECT_FALSE(block1a != block1a);
  EXPECT_FALSE(block1a != block1b);
  EXPECT_TRUE(block1a != block2);
  EXPECT_TRUE(block2 != block3);
  EXPECT_TRUE(block1a != block3);
}

}  // namespace web_package
