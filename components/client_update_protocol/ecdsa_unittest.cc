// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "components/client_update_protocol/ecdsa.h"

#include <stdint.h>

#include <limits>
#include <memory>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "crypto/random.h"
#include "crypto/secure_util.h"
#include "testing/gtest/include/gtest/gtest.h"

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

}  // end namespace

class CupEcdsaTest : public testing::Test {
 protected:
  Ecdsa& CUP() { return cup_; }

 private:
  Ecdsa cup_{8, kCupEcdsaTestKey};
};

TEST_F(CupEcdsaTest, SignRequest) {
  static const char kRequest[] = "TestSequenceForCupEcdsaUnitTest";
  static const char kRequestHash[] =
      "cde1f7dc1311ed96813057ca321c2f5a17ea2c9c776ee0eb31965f7985a3074a";
  static const char kRequestHashWithName[] =
      "&cup2hreq="
      "cde1f7dc1311ed96813057ca321c2f5a17ea2c9c776ee0eb31965f7985a3074a";
  static const char kKeyId[] = "8:";
  static const char kKeyIdWithName[] = "cup2key=8:";

  std::string query;
  CUP().SignRequest(kRequest, &query);
  std::string query2;
  CUP().SignRequest(kRequest, &query2);
  Ecdsa::RequestParameters request_parameters = CUP().SignRequest(kRequest);

  EXPECT_TRUE(base::StartsWith(query, kKeyIdWithName));
  EXPECT_TRUE(base::StartsWith(query2, kKeyIdWithName));
  EXPECT_TRUE(base::StartsWith(request_parameters.query_cup2key, kKeyId));
  EXPECT_TRUE(base::EndsWith(query, kRequestHashWithName));
  EXPECT_TRUE(base::EndsWith(query2, kRequestHashWithName));
  EXPECT_EQ(request_parameters.hash_hex, kRequestHash);

  // The nonce should be a base64url-encoded, 32-byte (256-bit) string.
  std::string_view nonce_b64 = query;
  nonce_b64.remove_prefix(strlen(kKeyIdWithName));
  nonce_b64.remove_suffix(strlen(kRequestHashWithName));
  std::string nonce;
  EXPECT_TRUE(base::Base64UrlDecode(
      nonce_b64, base::Base64UrlDecodePolicy::DISALLOW_PADDING, &nonce));
  EXPECT_EQ(32u, nonce.size());

  nonce_b64 = request_parameters.query_cup2key;
  nonce_b64.remove_prefix(strlen(kKeyId));
  EXPECT_TRUE(base::Base64UrlDecode(
      nonce_b64, base::Base64UrlDecodePolicy::DISALLOW_PADDING, &nonce));
  EXPECT_EQ(32u, nonce.size());

  nonce_b64 = query2;
  nonce_b64.remove_prefix(strlen(kKeyIdWithName));
  nonce_b64.remove_suffix(strlen(kRequestHashWithName));
  EXPECT_TRUE(base::Base64UrlDecode(
      nonce_b64, base::Base64UrlDecodePolicy::DISALLOW_PADDING, &nonce));
  EXPECT_EQ(32u, nonce.size());

  // With a 256-bit nonce, the probability of collision is negligible.
  EXPECT_NE(query, query2);
  EXPECT_NE(query, base::StringPrintf("cup2key=%s&cup2hreq=%s",
                                      request_parameters.query_cup2key.c_str(),
                                      request_parameters.hash_hex.c_str()));
}

TEST_F(CupEcdsaTest, ValidateResponse_TestETagParsing) {
  // Invalid ETags must be gracefully rejected without a crash.
  std::string query_discard;
  CUP().SignRequest("Request_A", &query_discard);
  CUP().OverrideNonceForTesting(8, 12345);

  // Expect a pass for a well-formed etag.
  EXPECT_TRUE(CUP().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));

  // Reject empty etags.
  EXPECT_FALSE(CUP().ValidateResponse("Response_A", ""));

  // Reject etags with zero-length hashes or signatures, even if the other
  // component is wellformed.
  EXPECT_FALSE(CUP().ValidateResponse("Response_A", ":"));
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":"));
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));

  // Reject etags with non-hex content in either component.
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458__ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":2727bc2b3c33feb6800a830f4055901d__7d65a84184c5fbeb3f816db0a243f5"));

  // Reject etags where either/both component has a length that's not a
  // multiple of 2 (i.e. not a valid hex encoding).
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f"));
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f"));

  // Reject etags where the hash is even, but not 256 bits.
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "3044"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5ff"));

  // Reject etags where the signature field is too small to be valid. (Note that
  // the case isn't even a signature -- it's a validly encoded ASN.1 NULL.)
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "0500"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));

  // Reject etags where the signature field is too big to be a valid signature.
  // (This is a validly formed structure, but both ints are over 256 bits.)
  EXPECT_FALSE(CUP().ValidateResponse(
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
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "0406020100020100"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "3044"
      "06200123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
      "06200123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));
  EXPECT_FALSE(CUP().ValidateResponse(
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
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "30"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "3000"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "3044"
      "02207fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "3044"
      "02207fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff00"
      "02207fb15d24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243"));
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "3044"
      "022000007f24e66c168ac150458c7ae51f843c4858e27d41be3f9396d4919bbd5656"
      "02202291bae598e4a41118ea1df24ce8494d4055b2842dc046e0223f5e17e86bd10e"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));
}

TEST_F(CupEcdsaTest, ValidateResponse_TestSigning) {
  std::string query_discard;
  CUP().SignRequest("Request_A", &query_discard);
  CUP().OverrideNonceForTesting(8, 12345);

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
  EXPECT_TRUE(CUP().ValidateResponse(
      "Response_A",
      "3045022077a2d004f1643a92af5d356877c3434c46519ce32882d6e30ef6d154ee9775e3"
      "022100aca63c77d34152bdc0918ae0629e82b59314e5459f607cdc5ac95f1a4b7c31a2"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));

  // Failure case: "Request_A" made it to the server intact, but the response
  // body is modified to "Response_B" on return.  The signature is now invalid.
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_B",
      "3045022077a2d004f1643a92af5d356877c3434c46519ce32882d6e30ef6d154ee9775e3"
      "022100aca63c77d34152bdc0918ae0629e82b59314e5459f607cdc5ac95f1a4b7c31a2"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));

  // Failure case: Request body was modified to "Request_B" before it reached
  // the server.  Test a fast reject based on the observed_hash parameter.
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_B",
      "304402206289a7765f0371c7c48796779747f1166707d5937a99af518845f44af95876"
      "8c0220139fe935fde3e6b416ee742f91c6a480113762d78d889a2661de37576866d21c"
      ":80e3ef1b373efe5f2a8383a0cf9c89fb2e0cbb8e85db4813655ff5dc05009e7e"));

  // Failure case: Request body was modified to "Request_B" before it reached
  // the server.  Test a slow reject based on a signature mismatch.
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_B",
      "304402206289a7765f0371c7c48796779747f1166707d5937a99af518845f44af95876"
      "8c0220139fe935fde3e6b416ee742f91c6a480113762d78d889a2661de37576866d21c"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));

  // Failure case: Request/response are intact, but the signature is invalid
  // because it was signed against a different nonce (67890).
  EXPECT_FALSE(CUP().ValidateResponse(
      "Response_A",
      "3046022100d3bbb1fb4451c8e04a07fe95404cc39121ed0e0bc084f87de19d52eee50a97"
      "bf022100dd7d41d467be2af98d9116b0c7ba09740d54578c02a02f74da5f089834be3403"
      ":2727bc2b3c33feb6800a830f4055901dd87d65a84184c5fbeb3f816db0a243f5"));
}

}  // namespace client_update_protocol
