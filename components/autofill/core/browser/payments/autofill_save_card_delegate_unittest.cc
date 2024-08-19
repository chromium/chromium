// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/autofill_save_card_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/metrics/payments/credit_card_save_metrics.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using InfoBarMetric = AutofillMetrics::InfoBarMetric;
using SaveCardOfferUserDecision =
    payments::PaymentsAutofillClient::SaveCardOfferUserDecision;
using UserProvidedCardDetails =
    payments::PaymentsAutofillClient::UserProvidedCardDetails;
using autofill_metrics::SaveCreditCardPromptResult;
using CardSaveType = payments::PaymentsAutofillClient::CardSaveType;

using UploadCallbackArgs =
    std::pair<SaveCardOfferUserDecision, UserProvidedCardDetails>;

const std::string kUserActionMetricNameLocal =
    "Autofill.CreditCardInfoBar.Local";
const std::string kUserActionMetricNameServer =
    "Autofill.CreditCardInfoBar.Server";

const std::string kPromptResultMetricNameLocal =
    "Autofill.CreditCardSaveFlowResult.Local";
const std::string kPromptResultMetricNameServer =
    "Autofill.CreditCardSaveFlowResult.Server";

const std::string kUserActionCvcMetricNameLocal = "Autofill.CvcInfoBar.Local";
const std::string kUserActionCvcMetricNameServer = "Autofill.CvcInfoBar.Upload";

const std::string kSaveWithCvcSuffix = ".SavingWithCvc";

// Params of AutofillSaveCardDelegateTest:
// -- bool is_upload: Indicates whether the card should be saved locally or
//                    uploaded to server.
class AutofillSaveCardDelegateTest : public ::testing::Test,
                                     public testing::WithParamInterface<bool> {
 protected:
  void LocalCallback(SaveCardOfferUserDecision decision);
  payments::PaymentsAutofillClient::LocalSaveCardPromptCallback
  MakeLocalCallback();
  void UploadCallback(SaveCardOfferUserDecision decision,
                      const UserProvidedCardDetails& user_card_details);
  payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
  MakeUploadCallback();
  autofill::AutofillSaveCardDelegate CreateDelegate(
      payments::PaymentsAutofillClient::SaveCreditCardOptions options = {});
  bool IsUpload() const { return GetParam(); }

  std::vector<SaveCardOfferUserDecision> local_offer_decisions_;
  std::vector<UploadCallbackArgs> upload_offer_decisions_;
};

void AutofillSaveCardDelegateTest::LocalCallback(
    SaveCardOfferUserDecision decision) {
  local_offer_decisions_.push_back(decision);
}

payments::PaymentsAutofillClient::LocalSaveCardPromptCallback
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

payments::PaymentsAutofillClient::UploadSaveCardPromptCallback
AutofillSaveCardDelegateTest::MakeUploadCallback() {
  return base::BindOnce(
      &AutofillSaveCardDelegateTest::UploadCallback,
      base::Unretained(this));  // Test function does not outlive test fixture.
}

autofill::AutofillSaveCardDelegate AutofillSaveCardDelegateTest::CreateDelegate(
    payments::PaymentsAutofillClient::SaveCreditCardOptions options) {
  if (IsUpload()) {
    return AutofillSaveCardDelegate(MakeUploadCallback(), options);
  }
  return AutofillSaveCardDelegate(MakeLocalCallback(), options);
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

INSTANTIATE_TEST_SUITE_P(All, AutofillSaveCardDelegateTest, testing::Bool());

TEST_P(AutofillSaveCardDelegateTest, RequiresFixFlowWithNameFix) {
  autofill::AutofillSaveCardDelegate delegate =
      CreateDelegate(payments::PaymentsAutofillClient::SaveCreditCardOptions{}
                         .with_should_request_name_from_user(true));
  EXPECT_TRUE(delegate.requires_fix_flow());
}

TEST_P(AutofillSaveCardDelegateTest, RequiresFixFlowWithExpirationDateFix) {
  autofill::AutofillSaveCardDelegate delegate =
      CreateDelegate(payments::PaymentsAutofillClient::SaveCreditCardOptions{}
                         .with_should_request_expiration_date_from_user(true));
  EXPECT_TRUE(delegate.requires_fix_flow());
}

TEST_P(AutofillSaveCardDelegateTest, RequiresFixFlowWithNoFix) {
  autofill::AutofillSaveCardDelegate delegate = CreateDelegate();
  EXPECT_FALSE(delegate.requires_fix_flow());
}

TEST_P(AutofillSaveCardDelegateTest,
       OnUiAcceptedWithCallbackArgumentRunsCallback) {
  base::MockOnceClosure mock_finish_gathering_consent_callback;
  EXPECT_CALL(mock_finish_gathering_consent_callback, Run).Times(1);
  CreateDelegate().OnUiAccepted(mock_finish_gathering_consent_callback.Get());
}

TEST_P(AutofillSaveCardDelegateTest, OnUiAcceptedRunsCallback) {
  CreateDelegate().OnUiAccepted();

  if (IsUpload()) {
    EXPECT_THAT(upload_offer_decisions_,
                testing::Contains(EqualToUploadCallbackArgs(
                    SaveCardOfferUserDecision::kAccepted, {})));
  } else {
    EXPECT_THAT(local_offer_decisions_,
                testing::Contains(SaveCardOfferUserDecision::kAccepted));
  }
}

TEST_P(AutofillSaveCardDelegateTest, OnUiAcceptedLogsPromptResult) {
  base::HistogramTester histogram_tester;

  CreateDelegate().OnUiAccepted();

  histogram_tester.ExpectUniqueSample(
      IsUpload() ? kPromptResultMetricNameServer : kPromptResultMetricNameLocal,
      SaveCreditCardPromptResult::kAccepted, 1);
}

TEST_P(
    AutofillSaveCardDelegateTest,
    OnUiAcceptedDoesNotLogPromptResultWhenUploadSaveRequestingExpirationDate) {
  // Upload-only feature, return early for local save.
  if (!IsUpload()) {
    return;
  }

  base::HistogramTester histogram_tester;

  CreateDelegate(
      /*options=*/{.should_request_expiration_date_from_user = true})
      .OnUiAccepted();

  histogram_tester.ExpectBucketCount(kPromptResultMetricNameServer,
                                     SaveCreditCardPromptResult::kAccepted, 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCardSaveFlowResult.Server.RequestingExpirationDate",
      SaveCreditCardPromptResult::kAccepted, 0);
}

TEST_P(AutofillSaveCardDelegateTest,
       OnUiAcceptedDoesNotLogPromptResultWhenUploadSaveRequestingName) {
  // Upload-only feature, return early for local save.
  if (!IsUpload()) {
    return;
  }

  base::HistogramTester histogram_tester;

  CreateDelegate(/*options=*/{.should_request_name_from_user = true})
      .OnUiAccepted();

  histogram_tester.ExpectBucketCount(kPromptResultMetricNameServer,
                                     SaveCreditCardPromptResult::kAccepted, 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCardSaveFlowResult.Server.RequestingCardholderName",
      SaveCreditCardPromptResult::kAccepted, 0);
}

TEST_P(AutofillSaveCardDelegateTest, OnUiAcceptedLogsUserAction) {
  base::HistogramTester histogram_tester;
  CreateDelegate().OnUiAccepted();
  histogram_tester.ExpectUniqueSample(
      IsUpload() ? kUserActionMetricNameServer : kUserActionMetricNameLocal,
      InfoBarMetric::INFOBAR_ACCEPTED, 1);
}

TEST_P(AutofillSaveCardDelegateTest, OnUiUpdatedAndAcceptedRunsUploadCallback) {
  // Upload-only feature, return early for local save.
  if (!IsUpload()) {
    return;
  }

  CreateDelegate().OnUiUpdatedAndAccepted(
      /*user_provided_details=*/{.cardholder_name = u"Test"});

  EXPECT_THAT(
      upload_offer_decisions_,
      testing::Contains(EqualToUploadCallbackArgs(
          SaveCardOfferUserDecision::kAccepted, {.cardholder_name = u"Test"})));
}

TEST_P(AutofillSaveCardDelegateTest, OnUiUpdatedAndAcceptedLogsUserAction) {
  // Upload-only feature, return early for local save.
  if (!IsUpload()) {
    return;
  }

  base::HistogramTester histogram_tester;

  CreateDelegate().OnUiUpdatedAndAccepted(/*user_provided_details=*/{});

  histogram_tester.ExpectUniqueSample(kUserActionMetricNameServer,
                                      InfoBarMetric::INFOBAR_ACCEPTED, 1);
}

TEST_P(AutofillSaveCardDelegateTest, OnUiCanceledRunsCallback) {
  CreateDelegate().OnUiCanceled();

  if (IsUpload()) {
    EXPECT_THAT(upload_offer_decisions_,
                testing::Contains(EqualToUploadCallbackArgs(
                    SaveCardOfferUserDecision::kDeclined, {})));
  } else {
    EXPECT_THAT(local_offer_decisions_,
                testing::Contains(SaveCardOfferUserDecision::kDeclined));
  }
}

TEST_P(AutofillSaveCardDelegateTest, OnUiCanceledLogsUserAction) {
  base::HistogramTester histogram_tester;

  CreateDelegate().OnUiCanceled();

  histogram_tester.ExpectUniqueSample(
      IsUpload() ? kUserActionMetricNameServer : kUserActionMetricNameLocal,
      InfoBarMetric::INFOBAR_DENIED, 1);
}

TEST_P(AutofillSaveCardDelegateTest, OnUiCanceledLogsPromptResult) {
  base::HistogramTester histogram_tester;

  CreateDelegate().OnUiCanceled();

  histogram_tester.ExpectUniqueSample(
      IsUpload() ? kPromptResultMetricNameServer : kPromptResultMetricNameLocal,
      SaveCreditCardPromptResult::kDenied, 1);
}

TEST_P(AutofillSaveCardDelegateTest, OnUiIgnoredRunsCallback) {
  CreateDelegate().OnUiIgnored();

  if (IsUpload()) {
    EXPECT_THAT(upload_offer_decisions_,
                testing::Contains(EqualToUploadCallbackArgs(
                    SaveCardOfferUserDecision::kIgnored, {})));
  } else {
    EXPECT_THAT(local_offer_decisions_,
                testing::Contains(SaveCardOfferUserDecision::kIgnored));
  }
}

TEST_P(AutofillSaveCardDelegateTest, OnUiIgnoredLogsUserAction) {
  base::HistogramTester histogram_tester;

  CreateDelegate().OnUiIgnored();

  histogram_tester.ExpectUniqueSample(
      IsUpload() ? kUserActionMetricNameServer : kUserActionMetricNameLocal,
      InfoBarMetric::INFOBAR_IGNORED, 1);
}

TEST_P(AutofillSaveCardDelegateTest, OnUiIgnoredLogsPromptResult) {
  base::HistogramTester histogram_tester;

  CreateDelegate().OnUiIgnored();

  histogram_tester.ExpectUniqueSample(
      IsUpload() ? kPromptResultMetricNameServer : kPromptResultMetricNameLocal,
      SaveCreditCardPromptResult::kIgnored, 1);
}

TEST_P(AutofillSaveCardDelegateTest, MetricsOnUiShownWhenCvcSave) {
  auto delegate = CreateDelegate(
      /*options=*/{.card_save_type = CardSaveType::kCvcSaveOnly});
  base::HistogramTester histogram_tester;
  delegate.OnUiShown();
  histogram_tester.ExpectUniqueSample(IsUpload()
                                          ? kUserActionCvcMetricNameServer
                                          : kUserActionCvcMetricNameLocal,
                                      AutofillMetrics::INFOBAR_SHOWN, 1);
}

TEST_P(AutofillSaveCardDelegateTest, MetricsOnUiAcceptedWhenCvcSave) {
  auto delegate = CreateDelegate(
      /*options=*/{.card_save_type = CardSaveType::kCvcSaveOnly});
  base::HistogramTester histogram_tester;
  delegate.OnUiAccepted();
  histogram_tester.ExpectUniqueSample(IsUpload()
                                          ? kUserActionCvcMetricNameServer
                                          : kUserActionCvcMetricNameLocal,
                                      InfoBarMetric::INFOBAR_ACCEPTED, 1);
}

TEST_P(AutofillSaveCardDelegateTest, MetricsOnUiIgnoredWhenCvcSave) {
  auto delegate = CreateDelegate(
      /*options=*/{.card_save_type = CardSaveType::kCvcSaveOnly});
  base::HistogramTester histogram_tester;
  delegate.OnUiIgnored();
  histogram_tester.ExpectUniqueSample(IsUpload()
                                          ? kUserActionCvcMetricNameServer
                                          : kUserActionCvcMetricNameLocal,
                                      InfoBarMetric::INFOBAR_IGNORED, 1);
}

TEST_P(AutofillSaveCardDelegateTest, MetricsOnUiCanceledCvcSave) {
  auto delegate = CreateDelegate(
      /*options=*/{.card_save_type = CardSaveType::kCvcSaveOnly});
  base::HistogramTester histogram_tester;
  delegate.OnUiCanceled();
  histogram_tester.ExpectUniqueSample(IsUpload()
                                          ? kUserActionCvcMetricNameServer
                                          : kUserActionCvcMetricNameLocal,
                                      InfoBarMetric::INFOBAR_DENIED, 1);
}

TEST_P(AutofillSaveCardDelegateTest, MetricsOnUiShownWhenSaveWithCvc) {
  auto delegate = CreateDelegate(
      /*options=*/{.card_save_type = CardSaveType::kCardSaveWithCvc});
  base::HistogramTester histogram_tester;
  delegate.OnUiShown();
  histogram_tester.ExpectUniqueSample(
      base::StrCat({IsUpload() ? kUserActionMetricNameServer
                               : kUserActionMetricNameLocal,
                    kSaveWithCvcSuffix}),
      AutofillMetrics::INFOBAR_SHOWN, 1);
}

TEST_P(AutofillSaveCardDelegateTest, MetricsOnUiAcceptedWhenSaveWithCvc) {
  auto delegate = CreateDelegate(
      /*options=*/{.card_save_type = CardSaveType::kCardSaveWithCvc});
  base::HistogramTester histogram_tester;
  delegate.OnUiAccepted();
  histogram_tester.ExpectUniqueSample(
      base::StrCat({IsUpload() ? kUserActionMetricNameServer
                               : kUserActionMetricNameLocal,
                    kSaveWithCvcSuffix}),
      InfoBarMetric::INFOBAR_ACCEPTED, 1);
}

TEST_P(AutofillSaveCardDelegateTest, MetricsOnUiIgnoredWhenSaveWithCvc) {
  auto delegate = CreateDelegate(
      /*options=*/{.card_save_type = CardSaveType::kCardSaveWithCvc});
  base::HistogramTester histogram_tester;
  delegate.OnUiIgnored();
  histogram_tester.ExpectUniqueSample(
      base::StrCat({IsUpload() ? kUserActionMetricNameServer
                               : kUserActionMetricNameLocal,
                    kSaveWithCvcSuffix}),
      InfoBarMetric::INFOBAR_IGNORED, 1);
}

TEST_P(AutofillSaveCardDelegateTest, MetricsOnUiCanceledWhenSaveWithCvc) {
  auto delegate = CreateDelegate(
      /*options=*/{.card_save_type = CardSaveType::kCardSaveWithCvc});
  base::HistogramTester histogram_tester;
  delegate.OnUiCanceled();
  histogram_tester.ExpectUniqueSample(
      base::StrCat({IsUpload() ? kUserActionMetricNameServer
                               : kUserActionMetricNameLocal,
                    kSaveWithCvcSuffix}),
      InfoBarMetric::INFOBAR_DENIED, 1);
}

}  // namespace autofill
