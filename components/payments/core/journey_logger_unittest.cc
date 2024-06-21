// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/journey_logger.h"

#include "base/metrics/metrics_hashes.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::ContainerEq;
using PaymentMethodCategory = payments::JourneyLogger::PaymentMethodCategory;
using Event2 = payments::JourneyLogger::Event2;

namespace payments {

int toInt(Event2 event) {
  return static_cast<int>(event);
}

// Tests that the completion status metrics based on whether the user had
// suggestions for all the requested sections are logged as correctly.
TEST(JourneyLoggerTest,
     RecordJourneyStatsHistograms_SuggestionsForEverything_Completed) {
  base::HistogramTester histogram_tester;
  JourneyLogger logger(ukm::kInvalidSourceId);

  // The merchant only requests payment information.
  logger.SetRequestedInformation(
      /*requested_shipping=*/false, /*requested_email=*/false,
      /*requested_phone=*/false, /*requested_name=*/false);
  std::vector<PaymentMethodCategory> methods{PaymentMethodCategory::kBasicCard,
                                             PaymentMethodCategory::kGoogle};
  logger.SetRequestedPaymentMethods(methods);

  // Simulate that the user had suggestions for all the requested sections.
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 1,
                                     /*has_complete_suggestion=*/false);

  // Simulate that the Payment Request was shown to the user.
  logger.SetShown();

  // Simulate that the user completes the checkout.
  logger.SetPayClicked();
  logger.SetSelectedMethod(PaymentMethodCategory::kBasicCard);
  logger.SetCompleted();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodOther));
}

// Tests that the completion status metrics based on whether the user had
// suggestions for all the requested sections are logged as correctly.
TEST(JourneyLoggerTest,
     RecordJourneyStatsHistograms_SuggestionsForEverything_UserAborted) {
  base::HistogramTester histogram_tester;
  JourneyLogger logger(ukm::kInvalidSourceId);

  // The merchant only requests payment information.
  logger.SetRequestedInformation(
      /*requested_shipping=*/false, /*requested_email=*/false,
      /*requested_phone=*/false, /*requested_name=*/false);
  std::vector<PaymentMethodCategory> methods{PaymentMethodCategory::kBasicCard,
                                             PaymentMethodCategory::kGoogle};
  logger.SetRequestedPaymentMethods(methods);

  // Simulate that the user had suggestions for all the requested sections.
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 1,
                                     /*has_complete_suggestion=*/false);

  // Simulate that the Payment Request was shown to the user.
  logger.SetShown();

  // Simulate that the user aborts the checkout.
  logger.SetAborted(JourneyLogger::ABORT_REASON_ABORTED_BY_USER);

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodOther));
}

// Tests that the completion status metrics based on whether the user had
// suggestions for all the requested sections are logged as correctly.
TEST(JourneyLoggerTest,
     RecordJourneyStatsHistograms_SuggestionsForEverything_OtherAborted) {
  base::HistogramTester histogram_tester;
  JourneyLogger logger(ukm::kInvalidSourceId);

  // The merchant only requests payment information.
  logger.SetRequestedInformation(
      /*requested_shipping=*/false, /*requested_email=*/false,
      /*requested_phone=*/false, /*requested_name=*/false);
  std::vector<PaymentMethodCategory> methods{PaymentMethodCategory::kBasicCard,
                                             PaymentMethodCategory::kGoogle};
  logger.SetRequestedPaymentMethods(methods);

  // Simulate that the user had suggestions for all the requested sections.
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 1,
                                     /*has_complete_suggestion=*/false);

  // Simulate that the Payment Request was shown to the user.
  logger.SetShown();

  // Simulate that the checkout is aborted.
  logger.SetAborted(JourneyLogger::ABORT_REASON_OTHER);

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodOther));
}

// Tests that the completion status metrics based on whether the user had
// suggestions for all the requested sections are logged as correctly, even in
// incognito mode.
TEST(JourneyLoggerTest,
     RecordJourneyStatsHistograms_SuggestionsForEverything_Incognito) {
  base::HistogramTester histogram_tester;
  JourneyLogger logger(ukm::kInvalidSourceId);

  // The merchant only requests payment information.
  logger.SetRequestedInformation(
      /*requested_shipping=*/false, /*requested_email=*/false,
      /*requested_phone=*/false, /*requested_name=*/false);
  std::vector<PaymentMethodCategory> methods{PaymentMethodCategory::kBasicCard,
                                             PaymentMethodCategory::kGoogle};
  logger.SetRequestedPaymentMethods(methods);

  // Simulate that the user had suggestions for all the requested sections.
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 1,
                                     /*has_complete_suggestion=*/false);

  // Simulate that the Payment Request was shown to the user.
  logger.SetShown();

  // Simulate that the user completes the checkout.
  logger.SetPayClicked();
  logger.SetSelectedMethod(PaymentMethodCategory::kBasicCard);
  logger.SetCompleted();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodOther));
}

// Tests that the completion status metrics based on whether the user had
// suggestions for all the requested sections are logged as correctly.
TEST(JourneyLoggerTest,
     RecordJourneyStatsHistograms_NoSuggestionsForEverything_Completed) {
  base::HistogramTester histogram_tester;
  JourneyLogger logger(ukm::kInvalidSourceId);

  // The merchant only requests payment information.
  logger.SetRequestedInformation(
      /*requested_shipping=*/false, /*requested_email=*/false,
      /*requested_phone=*/false, /*requested_name=*/false);
  std::vector<PaymentMethodCategory> methods{PaymentMethodCategory::kBasicCard,
                                             PaymentMethodCategory::kGoogle};
  logger.SetRequestedPaymentMethods(methods);

  // Simulate that the user had suggestions for none of the requested sections.
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 0,
                                     /*has_complete_suggestion=*/false);

  // Simulate that the Payment Request was shown to the user.
  logger.SetShown();

  // Simulate that the user completes the checkout.
  logger.SetPayClicked();
  logger.SetSelectedMethod(PaymentMethodCategory::kBasicCard);
  logger.SetCompleted();

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodOther));
}

// Tests that the completion status metrics based on whether the user had
// suggestions for all the requested sections are logged as correctly.
TEST(JourneyLoggerTest,
     RecordJourneyStatsHistograms_NoSuggestionsForEverything_UserAborted) {
  base::HistogramTester histogram_tester;
  JourneyLogger logger(ukm::kInvalidSourceId);

  // The merchant only requests payment information.
  logger.SetRequestedInformation(
      /*requested_shipping=*/false, /*requested_email=*/false,
      /*requested_phone=*/false, /*requested_name=*/false);
  std::vector<PaymentMethodCategory> methods{PaymentMethodCategory::kBasicCard,
                                             PaymentMethodCategory::kGoogle};
  logger.SetRequestedPaymentMethods(methods);

  // Simulate that the user had suggestions for none of the requested sections.
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 0,
                                     /*has_complete_suggestion=*/false);

  // Simulate that the Payment Request was shown to the user.
  logger.SetShown();

  // Simulate that the user aborts the checkout.
  logger.SetAborted(JourneyLogger::ABORT_REASON_ABORTED_BY_USER);

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodOther));
}

// Tests that the completion status metrics based on whether the user had
// suggestions for all the requested sections are logged as correctly.
TEST(JourneyLoggerTest,
     RecordJourneyStatsHistograms_NoSuggestionsForEverything_OtherAborted) {
  base::HistogramTester histogram_tester;
  JourneyLogger logger(ukm::kInvalidSourceId);

  // The merchant only requests payment information.
  logger.SetRequestedInformation(
      /*requested_shipping=*/false, /*requested_email=*/false,
      /*requested_phone=*/false, /*requested_name=*/false);
  std::vector<PaymentMethodCategory> methods{PaymentMethodCategory::kBasicCard,
                                             PaymentMethodCategory::kGoogle};
  logger.SetRequestedPaymentMethods(methods);

  // Simulate that the user had suggestions for none of the requested sections.
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 0,
                                     /*has_complete_suggestion=*/false);

  // Simulate that the Payment Request was shown to the user.
  logger.SetShown();

  // Simulate that the the checkout is aborted.
  logger.SetAborted(JourneyLogger::ABORT_REASON_OTHER);

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodOther));
}

// Tests that the completion status metrics based on whether the user had
// suggestions for all the requested sections are logged as correctly, even in
// incognito mode.
TEST(JourneyLoggerTest,
     RecordJourneyStatsHistograms_NoSuggestionsForEverything_Incognito) {
  base::HistogramTester histogram_tester;
  JourneyLogger logger(ukm::kInvalidSourceId);

  // The merchant only requests payment information.
  logger.SetRequestedInformation(
      /*requested_shipping=*/false, /*requested_email=*/false,
      /*requested_phone=*/false, /*requested_name=*/false);
  std::vector<PaymentMethodCategory> methods{PaymentMethodCategory::kBasicCard,
                                             PaymentMethodCategory::kGoogle};
  logger.SetRequestedPaymentMethods(methods);

  // Simulate that the user had suggestions for none of the requested sections.
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 0,
                                     /*has_complete_suggestion=*/false);

  // Simulate that the Payment Request was shown to the user.
  logger.SetShown();

  // Simulate that the user aborts the checkout.
  logger.SetAborted(JourneyLogger::ABORT_REASON_ABORTED_BY_USER);

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodOther));
}

// Tests that the completion status metrics based on whether the user had
// suggestions for all the requested sections are logged as correctly.
TEST(
    JourneyLoggerTest,
    RecordJourneyStatsHistograms_NoCompleteSuggestionsForEverything_OtherAborted) {
  base::HistogramTester histogram_tester;
  JourneyLogger logger(ukm::kInvalidSourceId);

  // The merchant only requests payment information.
  logger.SetRequestedInformation(
      /*requested_shipping=*/false, /*requested_email=*/false,
      /*requested_phone=*/false, /*requested_name=*/false);
  std::vector<PaymentMethodCategory> methods{PaymentMethodCategory::kBasicCard,
                                             PaymentMethodCategory::kGoogle};
  logger.SetRequestedPaymentMethods(methods);

  // Simulate that the user had incomplete suggestions for the requested
  // sections.
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 2,
                                     /*has_complete_suggestion=*/false);

  // Simulate that the Payment Request was shown to the user.
  logger.SetShown();

  // Simulate that the the checkout is aborted.
  logger.SetAborted(JourneyLogger::ABORT_REASON_OTHER);

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodOther));
}

// Tests that the completion status metrics based on whether the user had
// suggestions for all the requested sections are logged as correctly.
TEST(
    JourneyLoggerTest,
    RecordJourneyStatsHistograms_NoCompleteSuggestionsForEverything_SomeComplete_OtherAborted) {
  base::HistogramTester histogram_tester;
  JourneyLogger logger(ukm::kInvalidSourceId);

  // The merchant only requests payment information.
  logger.SetRequestedInformation(
      /*requested_shipping=*/true, /*requested_email=*/false,
      /*requested_phone=*/false, /*requested_name=*/false);
  std::vector<PaymentMethodCategory> methods{PaymentMethodCategory::kBasicCard,
                                             PaymentMethodCategory::kGoogle};
  logger.SetRequestedPaymentMethods(methods);

  // Simulate that the user had incomplete suggestions for one of the requested
  // sections.
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 2,
                                     /*has_complete_suggestion=*/false);
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_SHIPPING_ADDRESS, 1,
                                     /*has_complete_suggestion=*/true);

  // Simulate that the Payment Request was shown to the user.
  logger.SetShown();

  // Simulate that the the checkout is aborted.
  logger.SetAborted(JourneyLogger::ABORT_REASON_OTHER);

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodOther));
}

// Tests that the completion status metrics based on whether the user had
// suggestions for all the requested sections are logged as correctly.
TEST(
    JourneyLoggerTest,
    RecordJourneyStatsHistograms_CompleteSuggestionsForEverything_OtherAborted) {
  base::HistogramTester histogram_tester;
  JourneyLogger logger(ukm::kInvalidSourceId);

  // The merchant only requests payment information.
  logger.SetRequestedInformation(
      /*requested_shipping=*/true, /*requested_email=*/false,
      /*requested_phone=*/false, /*requested_name=*/false);
  std::vector<PaymentMethodCategory> methods{PaymentMethodCategory::kBasicCard,
                                             PaymentMethodCategory::kGoogle};
  logger.SetRequestedPaymentMethods(methods);

  // Simulate that the user had incomplete suggestions for one of the requested
  // sections.
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 2,
                                     /*has_complete_suggestion=*/true);
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_SHIPPING_ADDRESS, 1,
                                     /*has_complete_suggestion=*/true);

  // Simulate that the Payment Request was shown to the user.
  logger.SetShown();

  // Simulate that the the checkout is aborted.
  logger.SetAborted(JourneyLogger::ABORT_REASON_OTHER);

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodOther));
}

// Tests that the metrics are logged correctly for two simultaneous Payment
// Requests.
TEST(JourneyLoggerTest, RecordJourneyStatsHistograms_TwoPaymentRequests) {
  base::HistogramTester histogram_tester;
  JourneyLogger logger1(ukm::kInvalidSourceId);
  JourneyLogger logger2(ukm::kInvalidSourceId);

  // Make the two loggers have different data.
  logger1.SetShown();
  logger1.SetRequestedInformation(
      /*requested_shipping=*/true, /*requested_email=*/true,
      /*requested_phone=*/false, /*requested_name=*/false);

  std::vector<PaymentMethodCategory> methods{PaymentMethodCategory::kBasicCard,
                                             PaymentMethodCategory::kGoogle,
                                             PaymentMethodCategory::kOther};
  logger1.SetRequestedPaymentMethods(methods);

  logger2.SetShown();
  logger2.SetRequestedInformation(
      /*requested_shipping=*/true, /*requested_email=*/false,
      /*requested_phone=*/false, /*requested_name=*/false);
  std::vector<PaymentMethodCategory> methods2{PaymentMethodCategory::kOther};
  logger2.SetRequestedPaymentMethods(methods2);

  logger1.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 1,
                                      /*has_complete_suggestion=*/false);
  logger2.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 0,
                                      /*has_complete_suggestion=*/false);

  // Simulate that the user completes one checkout and aborts the other.
  logger1.SetPayClicked();
  logger1.SetSelectedMethod(PaymentMethodCategory::kBasicCard);
  logger1.SetCompleted();
  logger2.SetAborted(JourneyLogger::ABORT_REASON_ABORTED_BY_USER);

  // Make sure the correct events were logged.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(2U, buckets.size());
  // logger2
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kShown));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kOtherAborted));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kCompleted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kUserAborted));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestShipping));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestPayerData));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodOther));
  // logger1
  EXPECT_TRUE(buckets[1].min & toInt(Event2::kShown));
  EXPECT_TRUE(buckets[1].min & toInt(Event2::kHadInitialFormOfPayment));
  EXPECT_FALSE(buckets[1].min & toInt(Event2::kOtherAborted));
  EXPECT_TRUE(buckets[1].min & toInt(Event2::kCompleted));
  EXPECT_FALSE(buckets[1].min & toInt(Event2::kUserAborted));
  EXPECT_TRUE(buckets[1].min & toInt(Event2::kRequestShipping));
  EXPECT_TRUE(buckets[1].min & toInt(Event2::kRequestPayerData));
  EXPECT_TRUE(buckets[1].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_TRUE(buckets[1].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_TRUE(buckets[1].min & toInt(Event2::kRequestMethodOther));
}

// Tests that the Payment Request UKMs are logged correctly when the user aborts
// the Payment Request.
TEST(JourneyLoggerTest,
     RecordJourneyStatsHistograms_CheckoutFunnelUkm_UserAborted) {
  base::test::TaskEnvironment task_environment_;
  using UkmEntry = ukm::builders::PaymentRequest_CheckoutEvents;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  char test_url[] = "http://www.google.com/";

  base::HistogramTester histogram_tester;
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, GURL(test_url));
  JourneyLogger logger(source_id);
  logger.SetRequestedInformation(
      /*requested_shipping=*/true, /*requested_email=*/true,
      /*requested_phone=*/false, /*requested_name=*/false);
  std::vector<PaymentMethodCategory> methods{PaymentMethodCategory::kBasicCard};
  logger.SetRequestedPaymentMethods(methods);

  // Simulate that the user aborts after being shown the Payment Request and
  // clicking pay.
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 1,
                                     /*has_complete_suggestion=*/true);
  logger.SetShown();
  logger.SetPayClicked();
  logger.SetAborted(JourneyLogger::ABORT_REASON_ABORTED_BY_USER);

  int64_t expected_step_metric =
      toInt(Event2::kShown) | toInt(Event2::kPayClicked) |
      toInt(Event2::kRequestShipping) | toInt(Event2::kRequestPayerData) |
      toInt(Event2::kRequestMethodBasicCard) | toInt(Event2::kUserAborted) |
      toInt(Event2::kHadInitialFormOfPayment);

  // Make sure the UKM was logged correctly.
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    ukm_recorder.ExpectEntrySourceHasUrl(entry, GURL(test_url));
    EXPECT_EQ(2U, entry->metrics.size());
    ukm_recorder.ExpectEntryMetric(
        entry, UkmEntry::kCompletionStatusName,
        JourneyLogger::COMPLETION_STATUS_USER_ABORTED);
    ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kEvents2Name,
                                   expected_step_metric);
  }
}

// Tests that the Payment Request UKMs are logged correctly when the user
// completes the Payment Request.
TEST(JourneyLoggerTest,
     RecordJourneyStatsHistograms_CheckoutFunnelUkm_Completed) {
  base::test::TaskEnvironment task_environment_;
  using UkmEntry = ukm::builders::PaymentRequest_CheckoutEvents;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  char test_url[] = "http://www.google.com/";

  base::HistogramTester histogram_tester;
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, GURL(test_url));
  JourneyLogger logger(source_id);
  logger.SetRequestedInformation(
      /*requested_shipping=*/true, /*requested_email=*/true,
      /*requested_phone=*/false, /*requested_name=*/false);
  std::vector<PaymentMethodCategory> methods{PaymentMethodCategory::kBasicCard};
  logger.SetRequestedPaymentMethods(methods);

  // Simulate that the user aborts after being shown the Payment Request.
  logger.SetNumberOfSuggestionsShown(JourneyLogger::SECTION_PAYMENT_METHOD, 1,
                                     /*has_complete_suggestion=*/true);
  logger.SetShown();
  logger.SetPayClicked();
  logger.SetSelectedMethod(PaymentMethodCategory::kBasicCard);
  logger.SetCompleted();

  int64_t expected_step_metric =
      toInt(Event2::kShown) | toInt(Event2::kRequestShipping) |
      toInt(Event2::kRequestPayerData) |
      toInt(Event2::kRequestMethodBasicCard) | toInt(Event2::kCompleted) |
      toInt(Event2::kHadInitialFormOfPayment) | toInt(Event2::kPayClicked) |
      toInt(Event2::kSelectedCreditCard);

  // Make sure the UKM was logged correctly.
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    ukm_recorder.ExpectEntrySourceHasUrl(entry, GURL(test_url));
    EXPECT_EQ(2U, entry->metrics.size());
    ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kCompletionStatusName,
                                   JourneyLogger::COMPLETION_STATUS_COMPLETED);
    ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kEvents2Name,
                                   expected_step_metric);
  }
}

// Tests that the Payment Request UKMs and UMAs are logged correctly when the
// user selects the play billing method.
TEST(JourneyLoggerTest, RecordJourneyStatsHistograms_SelectedPlayBilling) {
  base::test::TaskEnvironment task_environment_;
  using UkmEntry = ukm::builders::PaymentRequest_CheckoutEvents;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  char test_url[] = "http://www.google.com/";

  base::HistogramTester histogram_tester;
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, GURL(test_url));
  JourneyLogger logger(source_id);
  std::vector<PaymentMethodCategory> methods{
      PaymentMethodCategory::kPlayBilling};
  logger.SetRequestedPaymentMethods(methods);
  logger.SetSkippedShow();
  logger.SetPayClicked();
  logger.SetSelectedMethod(PaymentMethodCategory::kPlayBilling);
  logger.SetCompleted();

  int64_t expected_events =
      toInt(Event2::kSkippedShow) | toInt(Event2::kCompleted) |
      toInt(Event2::kPayClicked) | toInt(Event2::kRequestMethodPlayBilling) |
      toInt(Event2::kSelectedPlayBilling);

  // Make sure the UKM was logged correctly.
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    ukm_recorder.ExpectEntrySourceHasUrl(entry, GURL(test_url));
    EXPECT_EQ(2U, entry->metrics.size());
    ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kCompletionStatusName,
                                   JourneyLogger::COMPLETION_STATUS_COMPLETED);
    ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kEvents2Name,
                                   expected_events);
  }

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodBasicCard));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodGoogle));
  EXPECT_FALSE(buckets[0].min &
               toInt(Event2::kRequestMethodGooglePayAuthentication));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kRequestMethodPlayBilling));
  EXPECT_FALSE(buckets[0].min &
               toInt(Event2::kRequestMethodSecurePaymentConfirmation));
  EXPECT_FALSE(buckets[0].min & toInt(Event2::kRequestMethodOther));
}

TEST(JourneyLoggerTest,
     RecordJourneyStatsHistograms_MethodGooglePayAuthentication) {
  base::test::TaskEnvironment task_environment_;
  using UkmEntry = ukm::builders::PaymentRequest_CheckoutEvents;
  ukm::TestAutoSetUkmRecorder ukm_recorder;
  char test_url[] = "http://www.google.com";

  base::HistogramTester histogram_tester;
  ukm::SourceId source_id = ukm::UkmRecorder::GetNewSourceID();
  ukm_recorder.UpdateSourceURL(source_id, GURL(test_url));
  JourneyLogger logger(source_id);
  std::vector<PaymentMethodCategory> methods{
      PaymentMethodCategory::kGooglePayAuthentication};
  logger.SetRequestedPaymentMethods(methods);
  logger.SetSkippedShow();
  logger.SetPayClicked();
  logger.SetSelectedMethod(PaymentMethodCategory::kGooglePayAuthentication);
  logger.SetCompleted();

  int64_t expected_events =
      toInt(Event2::kSkippedShow) | toInt(Event2::kCompleted) |
      toInt(Event2::kPayClicked) |
      toInt(Event2::kRequestMethodGooglePayAuthentication) |
      toInt(Event2::kSelectedOther);

  // Make sure the UKM was logged correctly.
  auto entries = ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* const entry : entries) {
    ukm_recorder.ExpectEntrySourceHasUrl(entry, GURL(test_url));
    EXPECT_EQ(2U, entry->metrics.size());
    ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kCompletionStatusName,
                                   JourneyLogger::COMPLETION_STATUS_COMPLETED);
    ukm_recorder.ExpectEntryMetric(entry, UkmEntry::kEvents2Name,
                                   expected_events);
  }

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  EXPECT_TRUE(buckets[0].min &
              toInt(Event2::kRequestMethodGooglePayAuthentication));
  EXPECT_TRUE(buckets[0].min & toInt(Event2::kSelectedOther));
}

}  // namespace payments
