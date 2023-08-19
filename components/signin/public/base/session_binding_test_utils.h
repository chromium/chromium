// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SESSION_BINDING_TEST_UTILS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SESSION_BINDING_TEST_UTILS_H_

#include "base/containers/span.h"
#include "base/strings/string_piece_forward.h"
#include "crypto/signature_verifier.h"

namespace signin {

// Verifies that `jwt` is well-formed and properly signed.
[[nodiscard]] bool VerifyJwtSignature(
    base::StringPiece jwt,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> public_key);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SESSION_BINDING_TEST_UTILS_H_
