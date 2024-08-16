// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/sharing_message/web_push/json_web_token_util.h"

#include <stdint.h>

#include <optional>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/json/json_reader.h"
#include "crypto/ec_private_key.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/bn.h"
#include "third_party/boringssl/src/include/openssl/ecdsa.h"
#include "third_party/boringssl/src/include/openssl/mem.h"

namespace {

// An ASN.1-encoded PrivateKeyInfo block from PKCS #8.
const char kPrivateKey[] =
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgS8wRbDOWz0lKExvIVQiRKtPAP8"
    "dgHUHAw5gyOd5d4jKhRANCAARZb49Va5MD/KcWtc0oiWc2e8njBDtQzj0mzcOl1fDSt16Pvu6p"
    "fTU3MTWnImDNnkPxtXm58K7Uax8jFxA4TeXJ";

TEST(JSONWebTokenUtilTest, VerifiesCreateJSONWebToken) {
  std::string private_key_info;
  ASSERT_TRUE(base::Base64Decode(kPrivateKey, &private_key_info));
  std::unique_ptr<crypto::ECPrivateKey> private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(std::vector<uint8_t>(
          private_key_info.begin(), private_key_info.end()));
  ASSERT_TRUE(private_key);

  // Create JWS.
  base::Value::Dict claims;
  claims.Set("aud", "https://chromium.org");
  std::optional<std::string> jwt =
      CreateJSONWebToken(claims.Clone(), private_key.get());
  ASSERT_TRUE(jwt);

  // Decompose JWS into data and signautre.
  std::string::size_type dot_position = jwt->rfind(".");
  ASSERT_NE(std::string::npos, dot_position);

  std::string data = jwt->substr(0, dot_position);
  std::string signature;
  ASSERT_TRUE(base::Base64UrlDecode(jwt->substr(dot_position + 1),
                                    base::Base64UrlDecodePolicy::IGNORE_PADDING,
                                    &signature));

  // Encode the signature.
  bssl::UniquePtr<ECDSA_SIG> ec_sig(ECDSA_SIG_new());
  ASSERT_TRUE(ec_sig);

  std::string::size_type half_size = signature.size() / 2;
  ASSERT_TRUE(BN_bin2bn((uint8_t*)signature.data(), half_size, ec_sig->r));
  ASSERT_TRUE(
      BN_bin2bn((uint8_t*)signature.data() + half_size, half_size, ec_sig->s));

  uint8_t* der;
  size_t der_len;
  ASSERT_TRUE(ECDSA_SIG_to_bytes(&der, &der_len, ec_sig.get()));
  std::vector<uint8_t> der_signature(der, der + der_len);
  OPENSSL_free(der);

  // Verify DER signature using SignatureVerifier.
  std::vector<uint8_t> public_key_info;
  ASSERT_TRUE(private_key->ExportPublicKey(&public_key_info));

  crypto::SignatureVerifier verifier;
  ASSERT_TRUE(verifier.VerifyInit(crypto::SignatureVerifier::ECDSA_SHA256,
                                  der_signature, public_key_info));

  verifier.VerifyUpdate(base::as_bytes(base::make_span(data)));
  ASSERT_TRUE(verifier.VerifyFinal());

  std::string::size_type data_dot_position = data.find(".");
  ASSERT_NE(std::string::npos, data_dot_position);

  // Verify header.
  std::string header_decoded;
  ASSERT_TRUE(base::Base64UrlDecode(data.substr(0, data_dot_position),
                                    base::Base64UrlDecodePolicy::IGNORE_PADDING,
                                    &header_decoded));
  std::optional<base::Value> header_value =
      base::JSONReader::Read(header_decoded);
  ASSERT_TRUE(header_value);
  ASSERT_TRUE(header_value->is_dict());
  ASSERT_EQ("ES256", *header_value->GetDict().FindString("alg"));
  ASSERT_EQ("JWT", *header_value->GetDict().FindString("typ"));

  // Verify payload.
  std::string payload_decoded;
  ASSERT_TRUE(base::Base64UrlDecode(data.substr(data_dot_position + 1),
                                    base::Base64UrlDecodePolicy::IGNORE_PADDING,
                                    &payload_decoded));
  ASSERT_EQ(claims, base::JSONReader::Read(payload_decoded));
}

}  // namespace
