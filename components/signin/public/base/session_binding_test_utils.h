// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_SESSION_BINDING_TEST_UTILS_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_SESSION_BINDING_TEST_UTILS_H_

#include <optional>
#include <string_view>

#include "base/containers/span.h"
#include "base/values.h"
#include "crypto/signature_verifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace signin {

// Verifies that `jwt` is well-formed and properly signed.
[[nodiscard]] testing::AssertionResult VerifyJwtSignature(
    std::string_view jwt,
    crypto::SignatureVerifier::SignatureAlgorithm algorithm,
    base::span<const uint8_t> public_key);

// Returns a parsed header part of `jwt` or std::nullopt if parsing fails.
std::optional<base::Value::Dict> ExtractHeaderFromJwt(std::string_view jwt);

// Returns a parsed payload part of `jwt` or std::nullopt if parsing fails.
std::optional<base::Value::Dict> ExtractPayloadFromJwt(std::string_view jwt);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_SESSION_BINDING_TEST_UTILS_H_
