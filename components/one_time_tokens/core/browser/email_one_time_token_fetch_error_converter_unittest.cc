// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/core/browser/email_one_time_token_fetch_error_converter.h"

#include "components/one_time_tokens/core/browser/fetch_email_one_time_token_error_details.pb.h"
#include "components/one_time_tokens/core/browser/one_time_token_retrieval_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace one_time_tokens {

using google::internal::chrome::passwords::onetimetoken::v1::
    FetchEmailOneTimeTokenErrorDetails;

TEST(EmailOneTimeTokenFetchErrorConverterTest, ConvertReason) {
  EXPECT_EQ(ConvertEmailOneTimeTokenFetchErrorReason(
                FetchEmailOneTimeTokenErrorDetails::REASON_CODE_UNSPECIFIED),
            OneTimeTokenRetrievalError::kGmailOtpBackendServerError);

  EXPECT_EQ(ConvertEmailOneTimeTokenFetchErrorReason(
                FetchEmailOneTimeTokenErrorDetails::
                    SMART_FEATURES_IN_GMAIL_CONSENT_IS_REQUIRED),
            OneTimeTokenRetrievalError::
                kGmailOtpBackendSmartFeaturesInGmailConsentRequired);
  EXPECT_EQ(
      ConvertEmailOneTimeTokenFetchErrorReason(
          FetchEmailOneTimeTokenErrorDetails::
              SMART_FEATURES_IN_OTHER_GOOGLE_PRODUCTS_CONSENT_IS_REQUIRED),
      OneTimeTokenRetrievalError::
          kGmailOtpBackendSmartFeaturesInOtherGoogleProductsConsentRequired);
  EXPECT_EQ(ConvertEmailOneTimeTokenFetchErrorReason(
                FetchEmailOneTimeTokenErrorDetails::
                    DMA_CROSS_PRODUCT_SHARING_CONSENT_IS_REQUIRED),
            OneTimeTokenRetrievalError::
                kGmailOtpBackendDmaCrossProductSharingConsentRequired);

  EXPECT_EQ(ConvertEmailOneTimeTokenFetchErrorReason(
                FetchEmailOneTimeTokenErrorDetails::BAD_MESSAGE_REFERENCE),
            OneTimeTokenRetrievalError::kGmailOtpBackendBadMessageReference);
  EXPECT_EQ(
      ConvertEmailOneTimeTokenFetchErrorReason(
          FetchEmailOneTimeTokenErrorDetails::WRONG_TOKEN_TYPE_REQUESTED),
      OneTimeTokenRetrievalError::kGmailOtpBackendWrongTokenTypeRequested);

  EXPECT_EQ(ConvertEmailOneTimeTokenFetchErrorReason(
                FetchEmailOneTimeTokenErrorDetails::MESSAGE_ID_NOT_FOUND),
            OneTimeTokenRetrievalError::kGmailOtpBackendMessageIdNotFound);
  EXPECT_EQ(ConvertEmailOneTimeTokenFetchErrorReason(
                FetchEmailOneTimeTokenErrorDetails::OTP_ATTRIBUTE_NOT_FOUND),
            OneTimeTokenRetrievalError::kGmailOtpBackendOtpAttributeNotFound);
  EXPECT_EQ(ConvertEmailOneTimeTokenFetchErrorReason(
                FetchEmailOneTimeTokenErrorDetails::ONE_TIME_TOKEN_EXPIRED),
            OneTimeTokenRetrievalError::kGmailOtpBackendOneTimeTokenExpired);
}

TEST(EmailOneTimeTokenFetchErrorConverterTest, ConvertDetailsEmpty) {
  FetchEmailOneTimeTokenErrorDetails details;
  EXPECT_EQ(ConvertEmailOneTimeTokenFetchErrorDetails(details),
            OneTimeTokenRetrievalError::kGmailOtpBackendServerError);
}

TEST(EmailOneTimeTokenFetchErrorConverterTest, ConvertDetailsSingle) {
  FetchEmailOneTimeTokenErrorDetails details;
  details.add_reason_code(FetchEmailOneTimeTokenErrorDetails::
                              SMART_FEATURES_IN_GMAIL_CONSENT_IS_REQUIRED);
  EXPECT_EQ(ConvertEmailOneTimeTokenFetchErrorDetails(details),
            OneTimeTokenRetrievalError::
                kGmailOtpBackendSmartFeaturesInGmailConsentRequired);
}

TEST(EmailOneTimeTokenFetchErrorConverterTest, ConvertDetailsMultiple) {
  FetchEmailOneTimeTokenErrorDetails details;
  // First is unspecified (fallback), second is consent. Should return consent.
  details.add_reason_code(
      FetchEmailOneTimeTokenErrorDetails::REASON_CODE_UNSPECIFIED);
  details.add_reason_code(FetchEmailOneTimeTokenErrorDetails::
                              SMART_FEATURES_IN_GMAIL_CONSENT_IS_REQUIRED);
  EXPECT_EQ(ConvertEmailOneTimeTokenFetchErrorDetails(details),
            OneTimeTokenRetrievalError::
                kGmailOtpBackendSmartFeaturesInGmailConsentRequired);
}

TEST(EmailOneTimeTokenFetchErrorConverterTest,
     ConvertDetailsMultipleAllFallback) {
  FetchEmailOneTimeTokenErrorDetails details;
  details.add_reason_code(
      FetchEmailOneTimeTokenErrorDetails::REASON_CODE_UNSPECIFIED);
  details.add_reason_code(
      FetchEmailOneTimeTokenErrorDetails::REASON_CODE_UNSPECIFIED);
  EXPECT_EQ(ConvertEmailOneTimeTokenFetchErrorDetails(details),
            OneTimeTokenRetrievalError::kGmailOtpBackendServerError);
}

}  // namespace one_time_tokens
