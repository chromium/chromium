// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_package/signed_web_bundles/ed25519_signature.h"

#include <vector>

#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "base/test/gmock_expected_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_package {

namespace {

constexpr uint8_t kMessage[] = {'t', 'e', 's', 't', ' ', 'm',
                                'e', 's', 's', 'a', 'g', 'e'};

constexpr uint8_t kOtherMessage[] = {'o', 't', 'h', 'e', 'r', ' ', 'm',
                                     'e', 's', 's', 'a', 'g', 'e'};

// Signature and public key were generated with the following Go script:
//
// package main
//
// import (
//  "crypto/ed25519"
//  "fmt"
//  "strings"
// )
//
// func printHex(data []byte) string {
//  var result strings.Builder
//  for i, byte := range data {
//   if i > 0 {
//    result.WriteString(", ")
//   }
//   fmt.Fprintf(&result, "0x%02x", byte)
//  }
//  return result.String()
// }
//
// func main() {
//  public_key, private_key, _ := ed25519.GenerateKey(nil)
//  fmt.Print("Public Key: ")
//  fmt.Println(printHex(public_key))
//  fmt.Print("Signature: ")
//  fmt.Println(printHex(ed25519.Sign(private_key, []byte("test message"))))
// }
constexpr uint8_t kPublicKey[] = {
    0xaf, 0x07, 0xe8, 0xc1, 0x25, 0xb3, 0xd2, 0x14, 0x61, 0x2a, 0x8b,
    0x9e, 0x1e, 0x42, 0x41, 0x5c, 0x72, 0x48, 0xc9, 0x4b, 0xd3, 0x30,
    0xb3, 0x17, 0x2d, 0xa0, 0xe0, 0xe6, 0x7b, 0xf7, 0xdd, 0x79};

constexpr uint8_t kSignature[] = {
    0x0e, 0x1d, 0x57, 0xf6, 0xd1, 0xa7, 0x59, 0xa5, 0x96, 0x06, 0x1e,
    0xe3, 0x8e, 0x41, 0xc2, 0x30, 0x5b, 0x68, 0x84, 0xae, 0x5b, 0xad,
    0x57, 0x49, 0x7f, 0x23, 0x76, 0x1c, 0xb2, 0x09, 0xc1, 0x4b, 0x5b,
    0xb7, 0x4d, 0x69, 0xea, 0x9d, 0x54, 0x32, 0x58, 0xe7, 0x60, 0xcc,
    0x88, 0x75, 0xb6, 0xd0, 0xe1, 0x40, 0x37, 0x44, 0x73, 0x6f, 0xbb,
    0x15, 0x1e, 0x36, 0x99, 0x4d, 0x8a, 0x1b, 0xb5, 0x0e};

// This is a different signature with the first byte changed to 0xFF.
constexpr uint8_t kOtherSignature[] = {
    0xFF, 0x1d, 0x57, 0xf6, 0xd1, 0xa7, 0x59, 0xa5, 0x96, 0x06, 0x1e,
    0xe3, 0x8e, 0x41, 0xc2, 0x30, 0x5b, 0x68, 0x84, 0xae, 0x5b, 0xad,
    0x57, 0x49, 0x7f, 0x23, 0x76, 0x1c, 0xb2, 0x09, 0xc1, 0x4b, 0x5b,
    0xb7, 0x4d, 0x69, 0xea, 0x9d, 0x54, 0x32, 0x58, 0xe7, 0x60, 0xcc,
    0x88, 0x75, 0xb6, 0xd0, 0xe1, 0x40, 0x37, 0x44, 0x73, 0x6f, 0xbb,
    0x15, 0x1e, 0x36, 0x99, 0x4d, 0x8a, 0x1b, 0xb5, 0x0e};

}  // namespace

TEST(Ed25519SignatureTest, ValidSignatureFromVector) {
  std::vector<uint8_t> bytes(64);
  bytes[3] = 123;

  ASSERT_OK_AND_ASSIGN(auto signature, Ed25519Signature::Create(bytes));
  EXPECT_TRUE(base::ranges::equal(bytes, signature.bytes()));
}

TEST(Ed25519SignatureTest, ValidSignatureFromArray) {
  auto signature = Ed25519Signature::Create(base::make_span(kSignature));
  EXPECT_TRUE(
      base::ranges::equal(base::make_span(kSignature), signature.bytes()));
}

TEST(Ed25519SignatureTest, Equality) {
  auto signature1a = Ed25519Signature::Create(base::make_span(kSignature));
  auto signature1b = Ed25519Signature::Create(base::make_span(kSignature));
  auto signature2 = Ed25519Signature::Create(base::make_span(kOtherSignature));
  EXPECT_TRUE(signature1a == signature1a);
  EXPECT_TRUE(signature1a == signature1b);
  EXPECT_FALSE(signature1a == signature2);

  EXPECT_FALSE(signature1a != signature1a);
  EXPECT_FALSE(signature1a != signature1b);
  EXPECT_TRUE(signature1a != signature2);
}

TEST(Ed25519SignatureTest, InvalidSignature) {
  std::vector<uint8_t> bytes(17);
  bytes[3] = 123;

  auto signature = Ed25519Signature::Create(bytes);
  EXPECT_THAT(signature,
              base::test::ErrorIs("The signature has the wrong length. "
                                  "Expected 64, but got 17 bytes."));
}

TEST(Ed25519SignatureTest, Verify) {
  auto signature = Ed25519Signature::Create(base::make_span(kSignature));

  EXPECT_TRUE(
      signature.Verify(base::make_span(kMessage),
                       Ed25519PublicKey::Create(base::make_span(kPublicKey))));

  EXPECT_FALSE(
      signature.Verify(base::make_span(kOtherMessage),
                       Ed25519PublicKey::Create(base::make_span(kPublicKey))));
}

}  // namespace web_package
