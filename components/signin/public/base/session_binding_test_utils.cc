// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/session_binding_test_utils.h"

#include "base/base64url.h"
#include "base/containers/span.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "crypto/signature_verifier.h"

namespace signin {

bool VerifyJwtSignature(base::StringPiece jwt,
                        crypto::SignatureVerifier::SignatureAlgorithm algorithm,
                        base::span<const uint8_t> public_key) {
  std::vector<base::StringPiece> parts = base::SplitStringPiece(
      jwt, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 3U) {
    return false;
  }
  std::string signature;
  if (!base::Base64UrlDecode(parts[2],
                             base::Base64UrlDecodePolicy::DISALLOW_PADDING,
                             &signature)) {
    return false;
  }

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(
          algorithm, base::as_bytes(base::make_span(signature)), public_key)) {
    return false;
  }
  verifier.VerifyUpdate(
      base::as_bytes(base::make_span(base::StrCat({parts[0], ".", parts[1]}))));
  return verifier.VerifyFinal();
}

}  // namespace signin
