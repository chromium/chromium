// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_ISSUER_CONFIG_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_ISSUER_CONFIG_H_

namespace private_verification_tokens {

inline constexpr char kIssuersKey[] = "issuers";
inline constexpr char kDomainKey[] = "domain";
inline constexpr char kVersionKey[] = "version";
inline constexpr char kPublicKeyKey[] = "public_key";
inline constexpr char kKeyIdKey[] = "key_id";
inline constexpr char kBatchSizeKey[] = "batch_size";
inline constexpr char kExpirationKey[] = "expiration";

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_ISSUER_CONFIG_H_
