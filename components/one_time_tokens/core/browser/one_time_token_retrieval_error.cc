// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"

#include <ostream>

namespace one_time_tokens {

std::ostream& operator<<(std::ostream& os, OneTimeTokenRetrievalError error) {
  switch (error) {
    case OneTimeTokenRetrievalError::kUnknown:
      return os << "kUnknown";
    case OneTimeTokenRetrievalError::kSmsOtpBackendError:
      return os << "kSmsOtpBackendError";
    case OneTimeTokenRetrievalError::kSmsOtpBackendTimeout:
      return os << "kSmsOtpBackendTimeout";
    case OneTimeTokenRetrievalError::kSmsOtpBackendPlatformNotSupported:
      return os << "kSmsOtpBackendPlatformNotSupported";
    case OneTimeTokenRetrievalError::kSmsOtpBackendApiNotAvailable:
      return os << "kSmsOtpBackendApiNotAvailable";
    case OneTimeTokenRetrievalError::kSmsOtpBackendUserPermissionRequired:
      return os << "kSmsOtpBackendUserPermissionRequired";
    case OneTimeTokenRetrievalError::kSmsOtpGmscoreVersionNotSupported:
      return os << "kSmsOtpGmscoreVersionNotSupported";
    case OneTimeTokenRetrievalError::kSmsOtpBackendInitializationFailed:
      return os << "kSmsOtpBackendInitializationFailed";
    case OneTimeTokenRetrievalError::kGmailOtpBackendAuthError:
      return os << "kGmailOtpBackendAuthError";
    case OneTimeTokenRetrievalError::kGmailOtpBackendNetworkError:
      return os << "kGmailOtpBackendNetworkError";
    case OneTimeTokenRetrievalError::kGmailOtpBackendInvalidResponse:
      return os << "kGmailOtpBackendInvalidResponse";
    case OneTimeTokenRetrievalError::kSmsOtpUnknown:
      return os << "kSmsOtpUnknown";
    case OneTimeTokenRetrievalError::kGmailOtpUnknown:
      return os << "kGmailOtpUnknown";
    case OneTimeTokenRetrievalError::kGmailOtpBackendApiNotAvailable:
      return os << "kGmailOtpBackendApiNotAvailable";
    case OneTimeTokenRetrievalError::kGmailOtpBackendInitializationFailed:
      return os << "kGmailOtpBackendInitializationFailed";
    case OneTimeTokenRetrievalError::
        kGmailOtpBackendSmartFeaturesInGmailConsentRequired:
      return os << "kGmailOtpBackendSmartFeaturesInGmailConsentRequired";
    case OneTimeTokenRetrievalError::
        kGmailOtpBackendSmartFeaturesInOtherGoogleProductsConsentRequired:
      return os << "kGmailOtpBackendSmartFeaturesInOtherGoogleProductsConsentRe"
                   "quired";
    case OneTimeTokenRetrievalError::
        kGmailOtpBackendDmaCrossProductSharingConsentRequired:
      return os << "kGmailOtpBackendDmaCrossProductSharingConsentRequired";
    case OneTimeTokenRetrievalError::kGmailOtpBackendBadMessageReference:
      return os << "kGmailOtpBackendBadMessageReference";
    case OneTimeTokenRetrievalError::kGmailOtpBackendMessageIdNotFound:
      return os << "kGmailOtpBackendMessageIdNotFound";
    case OneTimeTokenRetrievalError::kGmailOtpBackendWrongTokenTypeRequested:
      return os << "kGmailOtpBackendWrongTokenTypeRequested";
    case OneTimeTokenRetrievalError::kGmailOtpBackendOneTimeTokenExpired:
      return os << "kGmailOtpBackendOneTimeTokenExpired";
    case OneTimeTokenRetrievalError::kGmailOtpBackendOtpAttributeNotFound:
      return os << "kGmailOtpBackendOtpAttributeNotFound";
    case OneTimeTokenRetrievalError::kGmailOtpBackendServerError:
      return os << "kGmailOtpBackendServerError";
    case OneTimeTokenRetrievalError::kSubscriptionExpired:
      return os << "kSubscriptionExpired";
  }
  return os << static_cast<int>(error);
}

}  // namespace one_time_tokens
