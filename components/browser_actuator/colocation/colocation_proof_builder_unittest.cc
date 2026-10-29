// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_actuator/colocation/colocation_proof_builder.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/numerics/byte_conversions.h"
#include "components/browser_actuator/colocation/proto/chrome_colocation_proof.pb.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "google/protobuf/any.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_actuator {
namespace {

constexpr int64_t kFixedTimestampSeconds = 1790000000;
constexpr std::array<uint8_t, 4> kSignature = {0xde, 0xad, 0xbe, 0xef};

ChromeColocationProof UnpackProofFromAny(const std::string& serialized_any) {
  google::protobuf::Any any_proto;
  any_proto.ParseFromString(serialized_any);
  ChromeColocationProof proof;
  proof.ParseFromString(any_proto.value());
  return proof;
}

TEST(ColocationProofBuilderTest, PayloadEncodesLabelAndBigEndianSeconds) {
  // Spell out the 27-byte ASCII label literal ("Chrome co-location proof v1")
  // followed by the 8-byte big-endian representation of 0x0102030405060708 so
  // any accidental change to `kColocationProofLabel` fails this test.
  constexpr std::array<uint8_t, 35> kExpected = {
      'C', 'h', 'r', 'o',  'm',  'e',  ' ',  'c',  'o',  '-',  'l',  'o',
      'c', 'a', 't', 'i',  'o',  'n',  ' ',  'p',  'r',  'o',  'o',  'f',
      ' ', 'v', '1', 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
  };
  EXPECT_EQ(ColocationProofPayload(0x0102030405060708), kExpected);
}

TEST(ColocationProofBuilderTest, PayloadEncodesZeroSeconds) {
  constexpr std::array<uint8_t, 8> kZeroSeconds = {0, 0, 0, 0, 0, 0, 0, 0};
  std::array<uint8_t, kColocationProofPayloadSize> payload =
      ColocationProofPayload(0);
  EXPECT_EQ((base::span(payload).subspan<kColocationProofLabelSize, 8>()),
            base::span(kZeroSeconds));
}

TEST(ColocationProofBuilderTest, PayloadEncodesNegativeSeconds) {
  constexpr std::array<uint8_t, 8> kMinusOneSeconds = {0xff, 0xff, 0xff, 0xff,
                                                       0xff, 0xff, 0xff, 0xff};
  std::array<uint8_t, kColocationProofPayloadSize> payload =
      ColocationProofPayload(-1);
  EXPECT_EQ((base::span(payload).subspan<kColocationProofLabelSize, 8>()),
            base::span(kMinusOneSeconds));
}

TEST(ColocationProofBuilderTest, SerializeColocationProofAsAnySetsTypeUrl) {
  std::string serialized =
      SerializeColocationProofAsAny(kSignature, kFixedTimestampSeconds);
  google::protobuf::Any any_proto;
  any_proto.ParseFromString(serialized);
  EXPECT_EQ(any_proto.type_url(),
            "type.googleapis.com/browser_actuator.ChromeColocationProof");
}

TEST(ColocationProofBuilderTest, SerializeColocationProofAsAnyStoresSignature) {
  ChromeColocationProof proof = UnpackProofFromAny(
      SerializeColocationProofAsAny(kSignature, kFixedTimestampSeconds));
  EXPECT_EQ(base::as_byte_span(proof.signature()), base::span(kSignature));
}

TEST(ColocationProofBuilderTest, SerializeColocationProofAsAnyStoresTimestamp) {
  ChromeColocationProof proof = UnpackProofFromAny(
      SerializeColocationProofAsAny(kSignature, kFixedTimestampSeconds));
  EXPECT_EQ(proof.timestamp_seconds(), kFixedTimestampSeconds);
}

TEST(ColocationProofBuilderTest, SignatureVerifiesOverLabeledPayload) {
  crypto::keypair::PrivateKey private_key =
      crypto::keypair::PrivateKey::GenerateEcP256();
  crypto::keypair::PublicKey public_key =
      crypto::keypair::PublicKey::FromPrivateKey(private_key);
  std::vector<uint8_t> signature =
      crypto::sign::Sign(crypto::sign::SignatureKind::ECDSA_SHA256, private_key,
                         ColocationProofPayload(kFixedTimestampSeconds));
  ChromeColocationProof proof = UnpackProofFromAny(
      SerializeColocationProofAsAny(signature, kFixedTimestampSeconds));
  EXPECT_TRUE(crypto::sign::Verify(
      crypto::sign::SignatureKind::ECDSA_SHA256, public_key,
      ColocationProofPayload(proof.timestamp_seconds()),
      base::as_byte_span(proof.signature())));
}

TEST(ColocationProofBuilderTest,
     SignatureOverLabeledPayloadFailsAgainstBareTimestamp) {
  crypto::keypair::PrivateKey private_key =
      crypto::keypair::PrivateKey::GenerateEcP256();
  crypto::keypair::PublicKey public_key =
      crypto::keypair::PublicKey::FromPrivateKey(private_key);
  std::vector<uint8_t> signature =
      crypto::sign::Sign(crypto::sign::SignatureKind::ECDSA_SHA256, private_key,
                         ColocationProofPayload(kFixedTimestampSeconds));
  EXPECT_FALSE(crypto::sign::Verify(
      crypto::sign::SignatureKind::ECDSA_SHA256, public_key,
      base::I64ToBigEndian(kFixedTimestampSeconds), signature));
}

TEST(ColocationProofBuilderTest,
     SignatureOverLabeledPayloadFailsAgainstDifferentSeconds) {
  crypto::keypair::PrivateKey private_key =
      crypto::keypair::PrivateKey::GenerateEcP256();
  crypto::keypair::PublicKey public_key =
      crypto::keypair::PublicKey::FromPrivateKey(private_key);
  std::vector<uint8_t> signature =
      crypto::sign::Sign(crypto::sign::SignatureKind::ECDSA_SHA256, private_key,
                         ColocationProofPayload(kFixedTimestampSeconds));
  EXPECT_FALSE(crypto::sign::Verify(
      crypto::sign::SignatureKind::ECDSA_SHA256, public_key,
      ColocationProofPayload(kFixedTimestampSeconds + 1), signature));
}

}  // namespace
}  // namespace browser_actuator
