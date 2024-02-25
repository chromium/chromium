// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"

#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/filling_product.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::autofill_metrics {
namespace {

size_t GetTotalCountForPrefix(base::HistogramTester& histogram_tester,
                              const std::string& prefix) {
  size_t count = 0u;
  for (const auto& [histogram_name, histogram_count] :
       histogram_tester.GetTotalCountsForPrefix(prefix)) {
    count += histogram_count;
  }
  return count;
}

}  // anonymous namespace

TEST(SuggestionsListMetricsTest, LogSuggestionAcceptedIndex_CreditCard) {
  const int selected_suggestion_index = 2;

  base::HistogramTester histogram_tester;
  LogAutofillSuggestionAcceptedIndex(selected_suggestion_index,
                                     FillingProduct::kCreditCard,
                                     /*off_the_record=*/false);
  EXPECT_EQ(1u, GetTotalCountForPrefix(histogram_tester,
                                       "Autofill.SuggestionAcceptedIndex."));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.SuggestionAcceptedIndex.CreditCard"),
              BucketsAre(base::Bucket(selected_suggestion_index, 1)));
}

TEST(SuggestionsListMetricsTest, LogSuggestionAcceptedIndex_Profile) {
  const int selected_suggestion_index = 1;

  base::HistogramTester histogram_tester;
  LogAutofillSuggestionAcceptedIndex(selected_suggestion_index,
                                     FillingProduct::kAddress,
                                     /*off_the_record=*/false);

  EXPECT_EQ(1u, GetTotalCountForPrefix(histogram_tester,
                                       "Autofill.SuggestionAcceptedIndex."));
  EXPECT_THAT(histogram_tester.GetAllSamples(
                  "Autofill.SuggestionAcceptedIndex.Profile"),
              BucketsAre(base::Bucket(selected_suggestion_index, 1)));
}

TEST(SuggestionsListMetricsTest, LogSuggestionAcceptedIndex_Other) {
  const int selected_suggestion_index = 0;
  base::HistogramTester histogram_tester;
  LogAutofillSuggestionAcceptedIndex(selected_suggestion_index,
                                     FillingProduct::kNone,
                                     /*off_the_record=*/false);
  LogAutofillSuggestionAcceptedIndex(selected_suggestion_index,
                                     FillingProduct::kPassword,
                                     /*off_the_record=*/false);

  EXPECT_EQ(2u, GetTotalCountForPrefix(histogram_tester,
                                       "Autofill.SuggestionAcceptedIndex."));
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SuggestionAcceptedIndex.Other"),
      BucketsAre(base::Bucket(selected_suggestion_index, 2)));
}

TEST(SuggestionsListMetricsTest, LogAutofillSelectedManageEntry_Addresses) {
  base::HistogramTester histogram_tester;
  LogAutofillSelectedManageEntry(FillingProduct::kAddress);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SuggestionsListManageClicked"),
      BucketsAre(base::Bucket(ManageSuggestionType::kAddresses, 1)));
}

TEST(SuggestionsListMetricsTest,
     LogAutofillSelectedManageEntry_PaymentMethodsCreditCards) {
  base::HistogramTester histogram_tester;
  LogAutofillSelectedManageEntry(FillingProduct::kCreditCard);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SuggestionsListManageClicked"),
      BucketsAre(
          base::Bucket(ManageSuggestionType::kPaymentMethodsCreditCards, 1)));
}

TEST(SuggestionsListMetricsTest,
     LogAutofillSelectedManageEntry_PaymentMethodsIbans) {
  base::HistogramTester histogram_tester;
  LogAutofillSelectedManageEntry(FillingProduct::kIban);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SuggestionsListManageClicked"),
      BucketsAre(base::Bucket(ManageSuggestionType::kPaymentMethodsIbans, 1)));
}

TEST(SuggestionsListMetricsTest, LogAutofillSelectedManageEntry_Other) {
  base::HistogramTester histogram_tester;
  LogAutofillSelectedManageEntry(FillingProduct::kNone);
  LogAutofillSelectedManageEntry(FillingProduct::kPassword);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.SuggestionsListManageClicked"),
      BucketsAre(base::Bucket(ManageSuggestionType::kOther, 2)));
}

}  // namespace autofill::autofill_metrics
