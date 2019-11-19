// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/json_web_token_util.h"

#include <stdint.h>

#include "base/base64url.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"

namespace {
const char kKeyAlg[] = "alg";
const char kAlgES256[] = "ES256";

const char kKeyTyp[] = "typ";
const char kTypJwt[] = "JWT";
}  // namespace

namespace gcm {

base::Optional<std::string> CreateJSONWebToken(
    const base::Value& claims,
    crypto::ECPrivateKey* private_key) {
  if (!claims.is_dict()) {
    LOG(ERROR) << "claims is not a dictionary";
    return base::nullopt;
  }

  // Generate header.
  base::Value header(base::Value::Type::DICTIONARY);
  header.SetKey(kKeyAlg, base::Value(kAlgES256));
  header.SetKey(kKeyTyp, base::Value(kTypJwt));

  // Serialize header.
  std::string header_serialized;
  if (!base::JSONWriter::Write(header, &header_serialized)) {
    LOG(ERROR) << "Failed to write header as JSON";
    return base::nullopt;
  }
  std::string header_base64;
  base::Base64UrlEncode(header_serialized,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &header_base64);

  // Serialize claims as payload.
  std::string payload_serialized;
  if (!base::JSONWriter::Write(claims, &payload_serialized)) {
    LOG(ERROR) << "Failed to write claims as JSON";
    return base::nullopt;
  }
  std::string payload_base64;
  base::Base64UrlEncode(payload_serialized,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &payload_base64);
  std::string data = base::StrCat({header_base64, ".", payload_base64});

  // Create signature.
  auto signer = crypto::ECSignatureCreator::Create(private_key);
  std::vector<uint8_t> der_signature, raw_signature;
  if (!signer->Sign((const uint8_t*)data.data(), data.size(), &der_signature)) {
    LOG(ERROR) << "Failed to create DER signature";
    return base::nullopt;
  }
  if (!signer->DecodeSignature(der_signature, &raw_signature)) {
    LOG(ERROR) << "Failed to decode DER signature";
    return base::nullopt;
  }

  // Serialize signature.
  std::string signature_base64;
  base::Base64UrlEncode(std::string(raw_signature.begin(), raw_signature.end()),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &signature_base64);

  return base::StrCat({data, ".", signature_base64});
}

}  // namespace gcm
