// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/client_update_protocol/cup.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <string_view>

#include "base/base64url.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace client_update_protocol {

namespace {

// ML-DSA-44 PKCS#8 PrivateKeyInfo block (FIPS 204).
//
// How to generate this key:
//   openssl genpkey -algorithm ML-DSA-44 -outform DER -out mldsa44_priv.der
//   and use xxd -i to convert it to comma-separated hex.
constexpr auto kCupTestPrivateKeyInfo = std::to_array<uint8_t>({
    0x30, 0x34, 0x02, 0x01, 0x00, 0x30, 0x0b, 0x06, 0x09, 0x60, 0x86,
    0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x11, 0x04, 0x22, 0x80, 0x20,
    0x32, 0x0b, 0x8f, 0x1f, 0xd0, 0x18, 0x68, 0xf4, 0xeb, 0x50, 0xad,
    0x05, 0xd8, 0x5e, 0x28, 0x24, 0x22, 0x0e, 0xcb, 0x2e, 0xa9, 0x2e,
    0x3f, 0x76, 0x93, 0x29, 0x43, 0x1b, 0xf5, 0xdc, 0xe6, 0xb2,
});

}  // end namespace

class CupTest : public testing::Test {
 protected:
  Cup& cup() { return cup_; }

  std::string SignResponse(std::string_view request_body,
                           std::string_view response_body,
                           std::string_view cup2key_params) {
    auto req_hash = crypto::hash::Sha256(base::as_byte_span(request_body));
    auto resp_hash = crypto::hash::Sha256(base::as_byte_span(response_body));

    crypto::hash::Hasher hasher(crypto::hash::HashKind::kSha256);
    hasher.Update(req_hash);
    hasher.Update(resp_hash);
    hasher.Update(base::as_byte_span(cup2key_params));
    std::array<uint8_t, crypto::hash::kSha256Size> inner_hash = {};
    hasher.Finish(inner_hash);

    auto signature_bytes = crypto::sign::Sign(
        crypto::sign::SignatureKind::MLDSA_44, priv_key_, inner_hash);
    return base::StrCat({base::HexEncodeLower(signature_bytes), ":",
                         base::HexEncodeLower(req_hash)});
  }

  std::string GetCup2KeyParams(std::string_view req_params) {
    size_t start = req_params.find("cup2key=") + 8;
    size_t end = req_params.find("&", start);
    return std::string(req_params.substr(start, end - start));
  }

 private:
  crypto::keypair::PrivateKey priv_key_ =
      *crypto::keypair::PrivateKey::FromPrivateKeyInfo(kCupTestPrivateKeyInfo);
  Cup cup_{16, crypto::keypair::PublicKey::FromPrivateKey(priv_key_)
                   .ToSubjectPublicKeyInfo()};
};

void CupTestOneInputDoesNotCrash(std::string params,
                                 std::string response_body,
                                 std::string signature) {
  auto priv =
      crypto::keypair::PrivateKey::FromPrivateKeyInfo(kCupTestPrivateKeyInfo);
  const auto kPubKeyBytes = crypto::keypair::PublicKey::FromPrivateKey(*priv)
                                .ToSubjectPublicKeyInfo();

  client_update_protocol::Cup cup(16, kPubKeyBytes);
  cup.PrepareRequestParameters(params);
  cup.ValidateResponse(response_body, signature);
}

FUZZ_TEST(CupFuzzTest, CupTestOneInputDoesNotCrash);

TEST_F(CupTest, PrepareRequestParameters) {
  static constexpr std::string_view kRequest =
      "TestSequenceForCupMldsa44UnitTest";
  static constexpr std::string_view kRequestHashWithName =
      "&cup2hreq="
      "6cc674cc7d21d2eb7aac815fcf2814a793bac092f326f0a65cff0b539269099e";
  static constexpr std::string_view kKeyIdWithName = "cup2key=ML-DSA-44-16:";

  base::flat_set<std::string> queries;
  for (int i = 0; i < 3; ++i) {
    const std::string query = cup().PrepareRequestParameters(kRequest);
    // With a 256-bit nonce, the probability of collision is negligible.
    EXPECT_FALSE(queries.contains(query));
    queries.insert(query);
    EXPECT_TRUE(query.starts_with(kKeyIdWithName));
    EXPECT_TRUE(query.ends_with(kRequestHashWithName));
    // The nonce is a base64url-encoded, 32-byte (256-bit) string.
    std::string_view nonce_b64 = query;
    nonce_b64.remove_prefix(kKeyIdWithName.size());
    nonce_b64.remove_suffix(kRequestHashWithName.size());
    std::string nonce;
    EXPECT_TRUE(base::Base64UrlDecode(
        nonce_b64, base::Base64UrlDecodePolicy::DISALLOW_PADDING, &nonce));
    EXPECT_EQ(nonce.size(), 32u);
  }
}

TEST_F(CupTest, ValidateResponse_TestETagParsing) {
  // Invalid ETags are gracefully rejected without a crash.
  std::string valid_etag = SignResponse(
      "Request_A", "Response_A",
      GetCup2KeyParams(cup().PrepareRequestParameters("Request_A")));

  // Expect a pass for a well-formed etag.
  EXPECT_TRUE(cup().ValidateResponse("Response_A", valid_etag));

  // Reject empty etags.
  EXPECT_FALSE(cup().ValidateResponse("Response_A", ""));

  // Reject etags with zero-length hashes or signatures.
  EXPECT_FALSE(cup().ValidateResponse("Response_A", ":"));
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A", valid_etag.substr(0, valid_etag.find(':') + 1)));
  EXPECT_FALSE(cup().ValidateResponse("Response_A",
                                      valid_etag.substr(valid_etag.find(':'))));

  // Reject etags with non-hex content.
  std::string bad_hex_etag = valid_etag;
  bad_hex_etag[0] = '_';
  bad_hex_etag[1] = '_';
  EXPECT_FALSE(cup().ValidateResponse("Response_A", bad_hex_etag));

  std::string bad_hex_hash = valid_etag;
  bad_hex_hash[bad_hex_hash.size() - 1] = '_';
  bad_hex_hash[bad_hex_hash.size() - 2] = '_';
  EXPECT_FALSE(cup().ValidateResponse("Response_A", bad_hex_hash));

  // Reject etags where either component has an odd length.
  EXPECT_FALSE(cup().ValidateResponse("Response_A",
                                      valid_etag.substr(1)));  // odd signature
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A", valid_etag.substr(0, valid_etag.size() - 1)));

  // Reject etags where the hash is even, but not 256 bits (32 bytes = 64 hex
  // chars).
  std::string short_hash =
      valid_etag.substr(0, valid_etag.find(':') + 1) +
      "2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243";
  EXPECT_FALSE(cup().ValidateResponse("Response_A", short_hash));
  std::string long_hash = valid_etag + "ff";
  EXPECT_FALSE(cup().ValidateResponse("Response_A", long_hash));

  // Reject etags where the signature size is not 2420 bytes (4840 hex chars).
  std::string short_sig = valid_etag.substr(2);  // 2419 bytes
  EXPECT_FALSE(cup().ValidateResponse("Response_A", short_sig));
  std::string long_sig = "ff" + valid_etag;  // 2421 bytes
  EXPECT_FALSE(cup().ValidateResponse("Response_A", long_sig));
}

TEST_F(CupTest, ValidateResponse_TestSigning) {
  std::string query = cup().PrepareRequestParameters("Request_A");
  std::string cup2key = GetCup2KeyParams(query);

  // Valid case:
  std::string valid_etag = SignResponse("Request_A", "Response_A", cup2key);
  EXPECT_TRUE(cup().ValidateResponse("Response_A", valid_etag));

  // Failure case: Response modified.
  EXPECT_FALSE(cup().ValidateResponse("Response_B", valid_etag));

  // Failure case: Request modified (fast reject via hash mismatch in ETag).
  auto req_b_hash = crypto::hash::Sha256(base::as_byte_span("Request_B"));
  std::string bad_hash_etag = valid_etag.substr(0, valid_etag.find(':') + 1) +
                              base::HexEncodeLower(req_b_hash);
  EXPECT_FALSE(cup().ValidateResponse("Response_A", bad_hash_etag));

  // Failure case: Request modified (slow reject via signature mismatch).
  std::string sig_for_b = SignResponse("Request_B", "Response_A", cup2key);
  EXPECT_FALSE(cup().ValidateResponse("Response_A", sig_for_b));

  // Failure case: Wrong nonce (signed against a different nonce).
  std::string wrong_cup2key =
      base::StrCat({cup2key.substr(0, cup2key.find(':') + 1),
                    "0000000000000000000000000000000000000000000000000000000000"
                    "000000"});
  std::string wrong_nonce_etag =
      SignResponse("Request_A", "Response_A", wrong_cup2key);
  EXPECT_FALSE(cup().ValidateResponse("Response_A", wrong_nonce_etag));
}

}  // namespace client_update_protocol
