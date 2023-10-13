// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/autofill_save_card_delegate.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using InfoBarMetric = AutofillMetrics::InfoBarMetric;
using SaveCardOfferUserDecision = AutofillClient::SaveCardOfferUserDecision;
using UserProvidedCardDetails = AutofillClient::UserProvidedCardDetails;
using autofill_metrics::SaveCreditCardPromptResult;

using UploadCallbackArgs =
    std::pair<SaveCardOfferUserDecision, UserProvidedCardDetails>;

const std::string userActionMetricNameLocal =
    "Autofill.CreditCardInfoBar.Local";
const std::string userActionMetricNameServer =
    "Autofill.CreditCardInfoBar.Server";

const std::string promptResultMetricNameLocal =
    "Autofill.CreditCardSaveFlowResult.Local";
const std::string promptResultMetricNameServer =
    "Autofill.CreditCardSaveFlowResult.Server";

// TODO (crbug.com/1485194): Add tests for CVC save.
class AutofillSaveCardDelegateTest : public ::testing::Test {
 protected:
  void LocalCallback(SaveCardOfferUserDecision decision);
  AutofillClient::LocalSaveCardPromptCallback MakeLocalCallback();
  void UploadCallback(SaveCardOfferUserDecision decision,
                      const UserProvidedCardDetails& user_card_details);
  AutofillClient::UploadSaveCardPromptCallback MakeUploadCallback();

  std::vector<SaveCardOfferUserDecision> local_offer_decisions_;
  std::vector<UploadCallbackArgs> upload_offer_decisions_;
};

void AutofillSaveCardDelegateTest::LocalCallback(
    SaveCardOfferUserDecision decision) {
  local_offer_decisions_.push_back(decision);
}

AutofillClient::LocalSaveCardPromptCallback
AutofillSaveCardDelegateTest::MakeLocalCallback() {
  return base::BindOnce(
      &AutofillSaveCardDelegateTest::LocalCallback,
      base::Unretained(this));  // Test function does not outlive test fixture.
}

void AutofillSaveCardDelegateTest::UploadCallback(
    SaveCardOfferUserDecision decision,
    const UserProvidedCardDetails& user_card_details) {
  upload_offer_decisions_.emplace_back(decision, user_card_details);
}

AutofillClient::UploadSaveCardPromptCallback
AutofillSaveCardDelegateTest::MakeUploadCallback() {
  return base::BindOnce(
      &AutofillSaveCardDelegateTest::UploadCallback,
      base::Unretained(this));  // Test function does not outlive test fixture.
}

// Matcher of UserProvidedCardDetails matching equal fields.
MATCHER_P(EqualToUserProvidedCardDetails, details, "") {
  return details.cardholder_name == arg.cardholder_name &&
         details.expiration_date_month == arg.expiration_date_month &&
         details.expiration_date_year == arg.expiration_date_year;
}

// Matches a the UploadSaveCardPromptCallback arguments to an
// UploadCallbackArgs.
testing::Matcher<UploadCallbackArgs> EqualToUploadCallbackArgs(
    SaveCardOfferUserDecision decision,
    UserProvidedCardDetails details) {
  return testing::AllOf(
      testing::Field(&UploadCallbackArgs::first, decision),
      testing::Field(&UploadCallbackArgs::second,
                     EqualToUserProvidedCardDetails(details)));
}

TEST_F(AutofillSaveCardDelegateTest,
       OnUiAcceptedWithCallbackArgumentRunsCallback) {
  auto delegate = AutofillSaveCardDelegate(MakeLocalCallback(),
                                           /*options=*/{});

  base::MockOnceClosure mock_finish_gathering_consent_callback;
  EXPECT_CALL(mock_finish_gathering_consent_callback, Run).Times(1);
  delegate.OnUiAccepted(mock_finish_gathering_consent_callback.Get());
}

TEST_F(AutofillSaveCardDelegateTest, OnUiAcceptedRunsLocalCallback) {
  auto delegate = AutofillSaveCardDelegate(MakeLocalCallback(),
                                           /*options=*/{});

  delegate.OnUiAccepted();

  EXPECT_THAT(local_offer_decisions_,
              testing::Contains(SaveCardOfferUserDecision::kAccepted));
}

TEST_F(AutofillSaveCardDelegateTest, OnUiAcceptedRunsUploadCallback) {
  auto delegate = AutofillSaveCardDelegate(MakeUploadCallback(),
                                           /*options=*/{});

  delegate.OnUiAccepted();

  EXPECT_THAT(upload_offer_decisions_,
              testing::Contains(EqualToUploadCallbackArgs(
                  SaveCardOfferUserDecision::kAccepted, {})));
}

TEST_F(AutofillSaveCardDelegateTest, OnUiAcceptedLogsPromptResult) {
  auto delegate = AutofillSaveCardDelegate(MakeLocalCallback(),
                                           /*options=*/{});
  base::HistogramTester histogram_tester;

  delegate.OnUiAccepted();

  histogram_tester.ExpectUniqueSample(promptResultMetricNameLocal,
                                      SaveCreditCardPromptResult::kAccepted, 1);
}

TEST_F(AutofillSaveCardDelegateTest,
       OnUiAcceptedLogsPromptResultWhenUploadSave) {
  auto delegate = AutofillSaveCardDelegate(MakeUploadCallback(),
                                           /*options=*/{});
  base::HistogramTester histogram_tester;

  delegate.OnUiAccepted();

  histogram_tester.ExpectBucketCount(promptResultMetricNameServer,
                                     SaveCreditCardPromptResult::kAccepted, 1);
}

TEST_F(
    AutofillSaveCardDelegateTest,
    OnUiAcceptedDoesNotLogPromptResultWhenUploadSaveRequestingExpirationDate) {
  auto delegate = AutofillSaveCardDelegate(
      MakeUploadCallback(),
      /*options=*/{.should_request_expiration_date_from_user = true});
  base::HistogramTester histogram_tester;

  delegate.OnUiAccepted();

  histogram_tester.ExpectBucketCount(promptResultMetricNameServer,
                                     SaveCreditCardPromptResult::kAccepted, 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCardSaveFlowResult.Server.RequestingExpirationDate",
      SaveCreditCardPromptResult::kAccepted, 0);
}

TEST_F(AutofillSaveCardDelegateTest,
       OnUiAcceptedDoesNotLogPromptResultWhenUploadSaveRequestingName) {
  auto delegate = AutofillSaveCardDelegate(
      MakeUploadCallback(),
      /*options=*/{.should_request_name_from_user = true});
  base::HistogramTester histogram_tester;

  delegate.OnUiAccepted();

  histogram_tester.ExpectBucketCount(promptResultMetricNameServer,
                                     SaveCreditCardPromptResult::kAccepted, 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCardSaveFlowResult.Server.RequestingCardholderName",
      SaveCreditCardPromptResult::kAccepted, 0);
}

TEST_F(AutofillSaveCardDelegateTest, OnUiAcceptedLogsUserActionWhenLocalSave) {
  auto delegate = AutofillSaveCardDelegate(MakeLocalCallback(),
                                           /*options=*/{});
  base::HistogramTester histogram_tester;

  delegate.OnUiAccepted();

  histogram_tester.ExpectUniqueSample(userActionMetricNameLocal,
                                      InfoBarMetric::INFOBAR_ACCEPTED, 1);
}

TEST_F(AutofillSaveCardDelegateTest, OnUiAcceptedLogsUserActionWhenUploadSave) {
  auto delegate = AutofillSaveCardDelegate(MakeUploadCallback(),
                                           /*options=*/{});
  base::HistogramTester histogram_tester;

  delegate.OnUiAccepted();

  histogram_tester.ExpectUniqueSample(userActionMetricNameServer,
                                      InfoBarMetric::INFOBAR_ACCEPTED, 1);
}

TEST_F(AutofillSaveCardDelegateTest, OnUiUpdatedAndAcceptedRunsUploadCallback) {
  auto delegate = AutofillSaveCardDelegate(MakeUploadCallback(),
                                           /*options=*/{});

  delegate.OnUiUpdatedAndAccepted(
      /*user_provided_details=*/{.cardholder_name = u"Test"});

  EXPECT_THAT(
      upload_offer_decisions_,
      testing::Contains(EqualToUploadCallbackArgs(
          SaveCardOfferUserDecision::kAccepted, {.cardholder_name = u"Test"})));
}

TEST_F(AutofillSaveCardDelegateTest, OnUiUpdatedAndAcceptedLogsUserAction) {
  auto delegate = AutofillSaveCardDelegate(MakeUploadCallback(),
                                           /*options=*/{});
  base::HistogramTester histogram_tester;

  delegate.OnUiUpdatedAndAccepted(/*user_provided_details=*/{});

  histogram_tester.ExpectUniqueSample(userActionMetricNameServer,
                                      InfoBarMetric::INFOBAR_ACCEPTED, 1);
}

TEST_F(AutofillSaveCardDelegateTest, OnUiCanceledRunsUploadCallback) {
  auto delegate = AutofillSaveCardDelegate(MakeUploadCallback(),
                                           /*options=*/{});

  delegate.OnUiCanceled();

  EXPECT_THAT(upload_offer_decisions_,
              testing::Contains(EqualToUploadCallbackArgs(
                  SaveCardOfferUserDecision::kDeclined, /*details=*/{})));
}

TEST_F(AutofillSaveCardDelegateTest, OnUiCanceledLogsUserAction) {
  auto delegate = AutofillSaveCardDelegate(MakeUploadCallback(),
                                           /*options=*/{});
  base::HistogramTester histogram_tester;

  delegate.OnUiCanceled();

  histogram_tester.ExpectUniqueSample(userActionMetricNameServer,
                                      InfoBarMetric::INFOBAR_DENIED, 1);
}

TEST_F(AutofillSaveCardDelegateTest, OnUiCanceledLogsPromptResult) {
  auto delegate = AutofillSaveCardDelegate(MakeUploadCallback(),
                                           /*options=*/{});
  base::HistogramTester histogram_tester;

  delegate.OnUiCanceled();

  histogram_tester.ExpectUniqueSample(promptResultMetricNameServer,
                                      SaveCreditCardPromptResult::kDenied, 1);
}

TEST_F(AutofillSaveCardDelegateTest, OnUiIgnoredRunsUploadCallback) {
  auto delegate = AutofillSaveCardDelegate(MakeUploadCallback(),
                                           /*options=*/{});

  delegate.OnUiIgnored();

  EXPECT_THAT(upload_offer_decisions_,
              testing::Contains(EqualToUploadCallbackArgs(
                  SaveCardOfferUserDecision::kIgnored, /*details=*/{})));
}

TEST_F(AutofillSaveCardDelegateTest, OnUiIgnoredLogsUserAction) {
  auto delegate = AutofillSaveCardDelegate(MakeUploadCallback(),
                                           /*options=*/{});
  base::HistogramTester histogram_tester;

  delegate.OnUiIgnored();

  histogram_tester.ExpectUniqueSample(userActionMetricNameServer,
                                      InfoBarMetric::INFOBAR_IGNORED, 1);
}

TEST_F(AutofillSaveCardDelegateTest, OnUiIgnoredLogsPromptResult) {
  auto delegate = AutofillSaveCardDelegate(MakeUploadCallback(),
                                           /*options=*/{});
  base::HistogramTester histogram_tester;

  delegate.OnUiIgnored();

  histogram_tester.ExpectUniqueSample(promptResultMetricNameServer,
                                      SaveCreditCardPromptResult::kIgnored, 1);
}

}  // namespace autofill
