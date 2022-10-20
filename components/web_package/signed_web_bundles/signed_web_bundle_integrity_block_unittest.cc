// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/signed_web_bundle_integrity_block.h"

#include <utility>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

constexpr uint8_t kEd25519PublicKey1[32] = {0, 8, 8, 8, 0, 0, 0, 0, 0, 0, 0,
                                            0, 8, 0, 8, 0, 0, 0, 0, 0, 0, 0,
                                            0, 8, 8, 8, 0, 0, 0, 0, 0, 0};

constexpr uint8_t kEd25519PublicKey2[32] = {0, 8, 8, 8, 8, 8, 8, 0, 0, 0, 0,
                                            0, 8, 0, 8, 8, 0, 8, 0, 0, 0, 0,
                                            0, 8, 8, 8, 8, 8, 8, 0, 0, 0};

constexpr uint8_t kEd25519Signature1[64] = {
    0, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

constexpr uint8_t kEd25519Signature2[64] = {
    0, 0, 0, 0, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 3, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

constexpr uint8_t kCompleteEntryCbor1[] = {'e', 1, 1, 1};
constexpr uint8_t kCompleteEntryCbor2[] = {'e', 2, 2, 2};

constexpr uint8_t kAttributesCbor1[] = {'a', 1, 1, 1};
constexpr uint8_t kAttributesCbor2[] = {'a', 2, 2, 2};

mojom::BundleIntegrityBlockSignatureStackEntryPtr MakeSignatureStackEntry(
    base::span<const uint8_t> public_key,
    base::span<const uint8_t> signature,
    base::span<const uint8_t> complete_entry_cbor,
    base::span<const uint8_t> attributes_cbor) {
  auto raw_signature_stack_entry =
      mojom::BundleIntegrityBlockSignatureStackEntry::New();
  raw_signature_stack_entry->public_key =
      std::vector(std::begin(public_key), std::end(public_key));
  raw_signature_stack_entry->signature =
      std::vector(std::begin(signature), std::end(signature));
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
            "Cannot create an integrity block without any signatures.");
}

TEST(SignedWebBundleIntegrityBlockTest, SignatureStackEntryInvalidPublicKey) {
  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>
      raw_signature_stack;
  raw_signature_stack.push_back(MakeSignatureStackEntry(
      {}, kEd25519Signature1, kCompleteEntryCbor1, kAttributesCbor1));

  auto raw_integrity_block = mojom::BundleIntegrityBlock::New();
  raw_integrity_block->size = 42;
  raw_integrity_block->signature_stack = std::move(raw_signature_stack);

  auto integrity_block =
      SignedWebBundleIntegrityBlock::Create(std::move(raw_integrity_block));
  ASSERT_FALSE(integrity_block.has_value());
  EXPECT_EQ(integrity_block.error(),
            "Error while parsing signature stack entry: Invalid public key: "
            "The Ed25519 public key does not have the correct length. Expected "
            "32 bytes, but received 0 bytes.");
}

TEST(SignedWebBundleIntegrityBlockTest, SignatureStackEntryInvalidSignature) {
  std::vector<mojom::BundleIntegrityBlockSignatureStackEntryPtr>
      raw_signature_stack;
  raw_signature_stack.push_back(MakeSignatureStackEntry(
      kEd25519PublicKey1, {}, kCompleteEntryCbor1, kAttributesCbor1));

  auto raw_integrity_block = mojom::BundleIntegrityBlock::New();
  raw_integrity_block->size = 42;
  raw_integrity_block->signature_stack = std::move(raw_signature_stack);

  auto integrity_block =
      SignedWebBundleIntegrityBlock::Create(std::move(raw_integrity_block));
  ASSERT_FALSE(integrity_block.has_value());
  EXPECT_EQ(integrity_block.error(),
            "Error while parsing signature stack entry: Invalid signature: The "
            "signature has the wrong length. Expected 64, but got 0 bytes.");
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

  auto public_key_stack = integrity_block->GetPublicKeyStack();
  EXPECT_EQ(public_key_stack.size(), 1ul);
  EXPECT_TRUE(
      base::ranges::equal(public_key_stack[0].bytes(), kEd25519PublicKey1));

  auto signature_stack = integrity_block->signature_stack();
  EXPECT_EQ(signature_stack.size(), 1ul);
  EXPECT_TRUE(base::ranges::equal(signature_stack[0].public_key().bytes(),
                                  kEd25519PublicKey1));
  EXPECT_TRUE(base::ranges::equal(signature_stack[0].signature().bytes(),
                                  kEd25519Signature1));
  EXPECT_TRUE(base::ranges::equal(signature_stack[0].complete_entry_cbor(),
                                  kCompleteEntryCbor1));
  EXPECT_TRUE(base::ranges::equal(signature_stack[0].attributes_cbor(),
                                  kAttributesCbor1));
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
  auto public_key_stack = integrity_block->GetPublicKeyStack();
  EXPECT_EQ(public_key_stack.size(), 2ul);
  EXPECT_TRUE(
      base::ranges::equal(public_key_stack[0].bytes(), kEd25519PublicKey1));
  EXPECT_TRUE(
      base::ranges::equal(public_key_stack[1].bytes(), kEd25519PublicKey2));

  auto signature_stack = integrity_block->signature_stack();
  EXPECT_EQ(signature_stack.size(), 2ul);
  EXPECT_TRUE(base::ranges::equal(signature_stack[0].public_key().bytes(),
                                  kEd25519PublicKey1));
  EXPECT_TRUE(base::ranges::equal(signature_stack[0].signature().bytes(),
                                  kEd25519Signature1));
  EXPECT_TRUE(base::ranges::equal(signature_stack[0].complete_entry_cbor(),
                                  kCompleteEntryCbor1));
  EXPECT_TRUE(base::ranges::equal(signature_stack[0].attributes_cbor(),
                                  kAttributesCbor1));
  EXPECT_TRUE(base::ranges::equal(signature_stack[1].public_key().bytes(),
                                  kEd25519PublicKey2));
  EXPECT_TRUE(base::ranges::equal(signature_stack[1].signature().bytes(),
                                  kEd25519Signature2));
  EXPECT_TRUE(base::ranges::equal(signature_stack[1].complete_entry_cbor(),
                                  kCompleteEntryCbor2));
  EXPECT_TRUE(base::ranges::equal(signature_stack[1].attributes_cbor(),
                                  kAttributesCbor2));
}

}  // namespace web_package
