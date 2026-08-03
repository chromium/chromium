// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/client_update_protocol/cup.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "base/base64url.h"
#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/client_update_protocol/features.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace client_update_protocol {

namespace {

// How to generate this key:
//   openssl ecparam -genkey -name prime256v1 -out ecpriv.pem
//   openssl ec -in ecpriv.pem -pubout -out ecpub.pem
// and use xxd -i to convert it to comma-separated hex.
//
// If you change this key, you will also need to change all the test data.
constexpr auto kCupEcdsaTestKey = std::to_array<uint8_t>({
    0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
    0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
    0x42, 0x00, 0x04, 0x24, 0xd3, 0xa3, 0x2b, 0x23, 0x7a, 0x50, 0x7c, 0x94,
    0x1a, 0x41, 0xa8, 0xc3, 0xec, 0x42, 0x99, 0x0b, 0x61, 0x41, 0x75, 0x28,
    0xf6, 0xc7, 0x7b, 0x44, 0x85, 0xd2, 0xa5, 0x52, 0x0c, 0xef, 0xaf, 0x14,
    0x95, 0xb1, 0x9b, 0xff, 0x92, 0x6b, 0x9c, 0x84, 0xa3, 0x49, 0x87, 0xa3,
    0x09, 0xcf, 0xe9, 0xc5, 0x0a, 0x28, 0x23, 0xa9, 0x89, 0x4f, 0x35, 0x8b,
    0xde, 0x96, 0x5b, 0xe5, 0x30, 0x35, 0x0c,
});

// Post-Quantum Crypto (ML-DSA-44) PKCS#8 PrivateKeyInfo block (FIPS 204).
//
// How to generate this key:
//   openssl genpkey -algorithm ML-DSA-44 -outform DER -out mldsa44_priv.der
//   and use xxd -i to convert it to comma-separated hex.
constexpr auto kCupMldsa44TestPrivateKeyInfo = std::to_array<uint8_t>({
    0x30, 0x34, 0x02, 0x01, 0x00, 0x30, 0x0b, 0x06, 0x09, 0x60, 0x86,
    0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x11, 0x04, 0x22, 0x80, 0x20,
    0x32, 0x0b, 0x8f, 0x1f, 0xd0, 0x18, 0x68, 0xf4, 0xeb, 0x50, 0xad,
    0x05, 0xd8, 0x5e, 0x28, 0x24, 0x22, 0x0e, 0xcb, 0x2e, 0xa9, 0x2e,
    0x3f, 0x76, 0x93, 0x29, 0x43, 0x1b, 0xf5, 0xdc, 0xe6, 0xb2,
});

}  // end namespace

class CupEcdsaTest : public testing::Test {
 protected:
  Cup& cup() { return cup_; }

 private:
  Cup cup_{8, kCupEcdsaTestKey};
};

void EcdsaCupTestOneInputDoesNotCrash(std::string params,
                                      std::string response_body,
                                      std::string signature) {
  client_update_protocol::Cup cup(8, kCupEcdsaTestKey);
  cup.PrepareRequestParameters(params);
  cup.ValidateResponse(response_body, signature);
}

FUZZ_TEST(CupEcdsaFuzzTest, EcdsaCupTestOneInputDoesNotCrash);

TEST_F(CupEcdsaTest, PrepareRequestParameters) {
  static constexpr std::string_view kRequest =
      "TestSequenceForCupEcdsaUnitTest";
  static constexpr std::string_view kRequestHashWithName =
      "&cup2hreq="
      "cde1f7dc1311ed96813057ca321c2f5a17ea2c9c776ee0eb31965f7985a3074a";
  static constexpr std::string_view kKeyIdWithName = "cup2key=8:";

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

TEST_F(CupEcdsaTest, ValidateResponse_TestETagParsing) {
  // Invalid ETags are gracefully rejected without a crash.
  cup().PrepareRequestParameters("Request_A");
  cup().OverrideNonceForTesting(8, 12345);

  // Expect a pass for a well-formed etag.
  EXPECT_TRUE(cup().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));

  // Reject empty etags.
  EXPECT_FALSE(cup().ValidateResponse("Response_A", ""));

  // Reject etags with zero-length hashes or signatures, even if the other
  // component is wellformed.
  EXPECT_FALSE(cup().ValidateResponse("Response_A", ":"));
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":"));
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));

  // Reject etags with non-hex content in either component.
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458__ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":2727bc2b3c33feb6800a830f4055901d__7d65a84184c5fbeb3f816db0a243f5"));

  // Reject etags where either/both component has a length that's not a
  // multiple of 2 (i.e. not a valid hex encoding).
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f"));
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f"));

  // Reject etags where the hash is even, but not 256 bits.
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5ff"));

  // Reject etags where the signature field is too small to be valid. (Note that
  // the case isn't even a signature -- it's a validly encoded ASN.1 NULL.)
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "0500"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));

  // Reject etags where the signature field is too big to be a valid signature.
  // (This is a validly formed structure, but both ints are over 256 bits.)
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3048"
      "202207fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "202207fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5ff"));

  // Reject etags where the signature is valid DER-encoded ASN.1, but is not
  // an ECDSA signature. (This is actually stressing crypto's SignatureValidator
  // library, and not CUP's use of it, but it's worth testing here.)  Cases:
  // * Something that's not a sequence
  // * Sequences that contain things other than ints (i.e. octet strings)
  // * Sequences that contain a negative int.
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "0406020100020100"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3044"
      "06200123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
      "06200123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3046"
      "02047fffffff"
      "0220ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));

  // Reject etags where the signature is not a valid DER encoding. (Again, this
  // is stressing SignatureValidator.)  Test cases are:
  // * No length field
  // * Zero length field
  // * One of the ints has truncated content
  // * One of the ints has content longer than its length field
  // * A positive int is improperly zero-padded
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "30"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3000"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3044"
      "02207fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3044"
      "02207fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff00"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3044"
      "022000007f24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));
}

TEST_F(CupEcdsaTest, ValidateResponse_TestSigning) {
  cup().PrepareRequestParameters("Request_A");
  cup().OverrideNonceForTesting(8, 12345);

  // How to generate an ECDSA signature:
  //   echo -n Request_A | sha256sum | cut -d " " -f 1 > h
  //   echo -n Response_A | sha256sum | cut -d " " -f 1 >> h
  //   cat h | xxd -r -p > hbin
  //   echo -n 8:12345 >> hbin
  //   sha256sum hbin | cut -d " " -f 1 | xxd -r -p > hbin2
  //   openssl dgst -hex -sha256 -sign ecpriv.pem hbin2 | cut -d " " -f 2 > sig
  //   echo -n :Request_A | sha256sum | cut -d " " -f 1 >> sig
  //   cat sig
  // It's useful to throw this in a bash script and parameterize it if you're
  // updating this unit test.

  // Valid case:
  //  * Send "Request_A" with key 8 / nonce 12345 to server.
  //  * Receive "Response_A", signature, and observed request hash from server.
  //  * Signature signs HASH(Request_A) | HASH(Response_A) | 8:12345.
  //  * Observed hash matches HASH(Request_A).
  EXPECT_TRUE(cup().ValidateResponse(
      "Response_A",
      "3045022077a2d004f1643a92af5d356877c3434c46519ce32882d6e30ef6d154ee9775e3"
      "022100aca63c77d34152bdc0918ae0629e82b59314e5459f607cdc5ac95f1a4b7c31a2"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));

  // Failure case: "Request_A" made it to the server intact, but the response
  // body is modified to "Response_B" on return.  The signature is now invalid.
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_B",
      "3045022077a2d004f1643a92af5d356877c3434c46519ce32882d6e30ef6d154ee9775e3"
      "022100aca63c77d34152bdc0918ae0629e82b59314e5459f607cdc5ac95f1a4b7c31a2"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));

  // Failure case: Request body was modified to "Request_B" before it reached
  // the server.  Test a fast reject based on the observed_hash parameter.
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_B",
      "304402206289a7765f0371c7c48796779747f1166707d5937a99af518845f44af95876"
      "8c0220139fe935fde3e6b416ee742f91c6a480113762d78d889a2661de37576866d21c"
      ":80e3ef1b373efe5f2a8383a0cf9c89fb2e0cbb8e85db4813655ff5dc05009e7e"));

  // Failure case: Request body was modified to "Request_B" before it reached
  // the server.  Test a slow reject based on a signature mismatch.
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_B",
      "304402206289a7765f0371c7c48796779747f1166707d5937a99af518845f44af95876"
      "8c0220139fe935fde3e6b416ee742f91c6a480113762d78d889a2661de37576866d21c"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));

  // Failure case: Request/response are intact, but the signature is invalid
  // because it was signed against a different nonce (67890).
  EXPECT_FALSE(cup().ValidateResponse(
      "Response_A",
      "3046022100d3bbb1fb4451c8e04a07fe95404cc39121ed0e0bc084f87de19d52eee50a97"
      "bf022100dd7d41d467be2af98d9116b0c7ba09740d54578c02a02f74da5f089834be3403"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));
}

class CupMldsa44Test : public testing::Test {
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

  CupMldsa44Test() {
    scoped_feature_list_.InitAndEnableFeature(features::kPqcCupSigning);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  crypto::keypair::PrivateKey priv_key_ =
      *crypto::keypair::PrivateKey::FromPrivateKeyInfo(
          kCupMldsa44TestPrivateKeyInfo);
  Cup cup_{16, crypto::keypair::PublicKey::FromPrivateKey(priv_key_)
                   .ToSubjectPublicKeyInfo()};
};

void Mldsa44CupTestOneInputDoesNotCrash(std::string params,
                                        std::string response_body,
                                        std::string signature) {
  auto priv = crypto::keypair::PrivateKey::FromPrivateKeyInfo(
      kCupMldsa44TestPrivateKeyInfo);
  const auto kMldsaKeyPubBytes =
      crypto::keypair::PublicKey::FromPrivateKey(*priv)
          .ToSubjectPublicKeyInfo();

  client_update_protocol::Cup cup(16, kMldsaKeyPubBytes);
  cup.PrepareRequestParameters(params);
  cup.ValidateResponse(response_body, signature);
}

FUZZ_TEST(CupMldsa44FuzzTest, Mldsa44CupTestOneInputDoesNotCrash);

TEST_F(CupMldsa44Test, PrepareRequestParameters) {
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

TEST_F(CupMldsa44Test, ValidateResponse_TestETagParsing) {
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

TEST_F(CupMldsa44Test, ValidateResponse_TestSigning) {
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
