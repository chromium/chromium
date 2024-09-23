// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/web_push/json_web_token_util.h"

#include <stdint.h>

#include "base/base64url.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"

namespace {
const char kKeyAlg[] = "alg";
const char kAlgES256[] = "ES256";

const char kKeyTyp[] = "typ";
const char kTypJwt[] = "JWT";
}  // namespace

std::optional<std::string> CreateJSONWebToken(
    const base::Value::Dict& claims,
    crypto::ECPrivateKey* private_key) {
  // Generate header.
  base::Value::Dict header;
  header.Set(kKeyAlg, base::Value(kAlgES256));
  header.Set(kKeyTyp, base::Value(kTypJwt));

  // Serialize header.
  std::string header_serialized;
  if (!base::JSONWriter::Write(header, &header_serialized)) {
    LOG(ERROR) << "Failed to write header as JSON";
    return std::nullopt;
  }
  std::string header_base64;
  base::Base64UrlEncode(header_serialized,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &header_base64);

  // Serialize claims as payload.
  std::string payload_serialized;
  if (!base::JSONWriter::Write(claims, &payload_serialized)) {
    LOG(ERROR) << "Failed to write claims as JSON";
    return std::nullopt;
  }
  std::string payload_base64;
  base::Base64UrlEncode(payload_serialized,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &payload_base64);
  std::string data = base::StrCat({header_base64, ".", payload_base64});

  // Create signature.
  auto signer = crypto::ECSignatureCreator::Create(private_key);
  std::vector<uint8_t> der_signature, raw_signature;
  if (!signer->Sign(base::as_bytes(base::make_span(data)), &der_signature)) {
    LOG(ERROR) << "Failed to create DER signature";
    return std::nullopt;
  }
  if (!signer->DecodeSignature(der_signature, &raw_signature)) {
    LOG(ERROR) << "Failed to decode DER signature";
    return std::nullopt;
  }

  // Serialize signature.
  std::string signature_base64;
  base::Base64UrlEncode(std::string(raw_signature.begin(), raw_signature.end()),
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &signature_base64);

  return base::StrCat({data, ".", signature_base64});
}
