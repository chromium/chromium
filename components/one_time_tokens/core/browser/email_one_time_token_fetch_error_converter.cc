// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/email_one_time_token_fetch_error_converter.h"

#include "base/logging.h"

namespace one_time_tokens {

using google::internal::chrome::passwords::onetimetoken::v1::
    FetchEmailOneTimeTokenErrorDetails;

OneTimeTokenRetrievalError ConvertEmailOneTimeTokenFetchErrorReason(
    FetchEmailOneTimeTokenErrorDetails::ReasonCode reason_code) {
  switch (reason_code) {
    case FetchEmailOneTimeTokenErrorDetails::
        SMART_FEATURES_IN_GMAIL_CONSENT_IS_REQUIRED:
      return OneTimeTokenRetrievalError::
          kGmailOtpBackendSmartFeaturesInGmailConsentRequired;
    case FetchEmailOneTimeTokenErrorDetails::
        SMART_FEATURES_IN_OTHER_GOOGLE_PRODUCTS_CONSENT_IS_REQUIRED:
      return OneTimeTokenRetrievalError::
          kGmailOtpBackendSmartFeaturesInOtherGoogleProductsConsentRequired;
    case FetchEmailOneTimeTokenErrorDetails::
        DMA_CROSS_PRODUCT_SHARING_CONSENT_IS_REQUIRED:
      return OneTimeTokenRetrievalError::
          kGmailOtpBackendDmaCrossProductSharingConsentRequired;

    case FetchEmailOneTimeTokenErrorDetails::BAD_MESSAGE_REFERENCE:
      return OneTimeTokenRetrievalError::kGmailOtpBackendBadMessageReference;
    case FetchEmailOneTimeTokenErrorDetails::WRONG_TOKEN_TYPE_REQUESTED:
      return OneTimeTokenRetrievalError::
          kGmailOtpBackendWrongTokenTypeRequested;

    case FetchEmailOneTimeTokenErrorDetails::MESSAGE_ID_NOT_FOUND:
      return OneTimeTokenRetrievalError::kGmailOtpBackendMessageIdNotFound;
    case FetchEmailOneTimeTokenErrorDetails::OTP_ATTRIBUTE_NOT_FOUND:
      return OneTimeTokenRetrievalError::kGmailOtpBackendOtpAttributeNotFound;
    case FetchEmailOneTimeTokenErrorDetails::ONE_TIME_TOKEN_EXPIRED:
      return OneTimeTokenRetrievalError::kGmailOtpBackendOneTimeTokenExpired;

    case FetchEmailOneTimeTokenErrorDetails::REASON_CODE_UNSPECIFIED:
    default:
      return OneTimeTokenRetrievalError::kGmailOtpBackendServerError;
  }
}

OneTimeTokenRetrievalError ConvertEmailOneTimeTokenFetchErrorDetails(
    const FetchEmailOneTimeTokenErrorDetails& error_details) {
  VLOG(1) << "Converting Email One Time Token Fetch error details. Number of "
             "reasons: "
          << error_details.reason_code_size();

  if (error_details.reason_code_size() == 0) {
    VLOG(1) << "No reason codes provided. Falling back to "
               "kGmailOtpBackendServerError.";
    return OneTimeTokenRetrievalError::kGmailOtpBackendServerError;
  }

  for (int r : error_details.reason_code()) {
    VLOG(1) << "Raw Reason: " << r;
  }

  // Iterate and find the first error that is not the fallback ServerError.
  // If all are ServerError, return ServerError.
  for (int i = 0; i < error_details.reason_code_size(); ++i) {
    FetchEmailOneTimeTokenErrorDetails::ReasonCode reason =
        error_details.reason_code(i);
    OneTimeTokenRetrievalError error =
        ConvertEmailOneTimeTokenFetchErrorReason(reason);
    VLOG(1) << "Mapped Reason[" << i << "] to " << static_cast<int>(error);

    if (error != OneTimeTokenRetrievalError::kGmailOtpBackendServerError) {
      VLOG(1) << "Selected granular error: " << static_cast<int>(error);
      return error;
    }
  }

  VLOG(1) << "All reasons mapped to fallback. Returning "
             "kGmailOtpBackendServerError.";
  return OneTimeTokenRetrievalError::kGmailOtpBackendServerError;
}

}  // namespace one_time_tokens
