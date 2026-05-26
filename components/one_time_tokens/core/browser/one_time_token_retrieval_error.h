// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_RETRIEVAL_ERROR_H_
#define COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_RETRIEVAL_ERROR_H_

namespace one_time_tokens {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class OneTimeTokenRetrievalError {
  kUnknown = 0,
  // The following map to SmsOtpRetrievalApiError.
  kSmsOtpBackendError = 1,
  kSmsOtpBackendTimeout = 2,
  kSmsOtpBackendPlatformNotSupported = 3,
  kSmsOtpBackendApiNotAvailable = 4,
  kSmsOtpBackendUserPermissionRequired = 5,
  kSmsOtpGmscoreVersionNotSupported = 6,
  // Error code for when AndroidSmsOtpBackend initialization fails
  kSmsOtpBackendInitializationFailed = 7,
  kGmailOtpBackendAuthError = 8,
  kGmailOtpBackendNetworkError = 9,
  kGmailOtpBackendInvalidResponse = 10,
  kSmsOtpUnknown = 11,
  kGmailOtpUnknown = 12,
  kGmailOtpBackendApiNotAvailable = 13,
  kGmailOtpBackendInitializationFailed = 14,
  kGmailOtpBackendSmartFeaturesInGmailConsentRequired = 15,
  kGmailOtpBackendSmartFeaturesInOtherGoogleProductsConsentRequired = 16,
  kGmailOtpBackendDmaCrossProductSharingConsentRequired = 17,
  kGmailOtpBackendBadMessageReference = 18,
  kGmailOtpBackendMessageIdNotFound = 19,
  kGmailOtpBackendWrongTokenTypeRequested = 20,
  kGmailOtpBackendOneTimeTokenExpired = 21,
  kGmailOtpBackendOtpAttributeNotFound = 22,
  kGmailOtpBackendServerError = 23,
  kMaxValue = kGmailOtpBackendServerError,
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_CORE_BROWSER_ONE_TIME_TOKEN_RETRIEVAL_ERROR_H_
