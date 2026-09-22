// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_METRICS_H_
#define COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_METRICS_H_

namespace private_verification_tokens {

inline constexpr char kFeatureActiveHistogram[] =
    "Security.PrivateVerificationTokens.FeatureActive";
inline constexpr char kEmptyCacheHistogram[] =
    "Security.PrivateVerificationTokens.EmptyCache";
inline constexpr char kTokenParsingResultHistogram[] =
    "Security.PrivateVerificationTokens.TokenParsingResult";
inline constexpr char kFetchSetupResultHistogram[] =
    "Security.PrivateVerificationTokens.FetchSetupResult";
inline constexpr char kTokenFetchTimeHistogram[] =
    "Security.PrivateVerificationTokens.TokenFetchTime";
inline constexpr char kTokenAttachTimeHistogram[] =
    "Security.PrivateVerificationTokens.TokenAttachTime";
inline constexpr char kRedemptionLimitHitHistogram[] =
    "Security.PrivateVerificationTokens.RedemptionLimitHit";

// Outcome of initializing request fetchers before sending payload securely.
//
// LINT.IfChange(PrivateVerificationTokensFetchSetupResult)
enum class PrivateVerificationTokensFetchSetupResult {
  kSuccess = 0,
  kInvalidVersion = 1,
  kFetcherInitFailed = 2,
  kMaxValue = kFetcherInitFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/security/enums.xml:PrivateVerificationTokensFetchSetupResult)

// Outcome of parsing and finalizing token responses received from issuers.
//
// LINT.IfChange(PrivateVerificationTokensTokenParsingResult)
enum class PrivateVerificationTokensTokenParsingResult {
  kSuccess = 0,
  kNetError = 1,
  kNullResponse = 2,
  kInvalidBatchSize = 3,
  kInvalidBucketCount = 4,
  kClientRequestGenerationFailed = 5,
  kAlreadyFinalized = 6,
  kInvalidResponseBodyLength = 7,
  kClientFinalizeFailed = 8,
  kMaxValue = kClientFinalizeFailed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/security/enums.xml:PrivateVerificationTokensTokenParsingResult)

}  // namespace private_verification_tokens

#endif  // COMPONENTS_PRIVATE_VERIFICATION_TOKENS_COMMON_PRIVATE_VERIFICATION_TOKENS_METRICS_H_
