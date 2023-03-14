// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/test_autofill_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::Bucket;

namespace autofill::autofill_metrics {

namespace {

constexpr char kCardWithoutMetadataId[] =
    "10000000-0000-0000-0000-000000000001";
constexpr char kCardWithMetadataId[] = "10000000-0000-0000-0000-000000000002";

}  // namespace

// Params:
// 1) Whether card product name feature flag is enabled.
// 2) whether card art image feature flag is enabled.
// 3) Whether card metadata (both product name and card art image) are provided.
// 4) Whether the card has linked virtual card (only card art is provided).
class CardMetadataMetricsTest
    : public AutofillMetricsBaseTest,
      public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
 public:
  CardMetadataMetricsTest()
      : card_product_name_enabled_(std::get<0>(GetParam())),
        card_art_image_enabled_(std::get<1>(GetParam())),
        card_metadata_available_(std::get<2>(GetParam())),
        card_has_linked_virtual_card_(std::get<3>(GetParam())) {
    feature_list_card_product_name_.InitWithFeatureState(
        features::kAutofillEnableCardProductName, card_product_name_enabled_);
    feature_list_card_art_image_.InitWithFeatureState(
        features::kAutofillEnableCardArtImage, card_art_image_enabled_);
  }
  ~CardMetadataMetricsTest() override = default;

  bool card_product_name_enabled() { return card_product_name_enabled_; }
  bool card_art_image_enabled() { return card_art_image_enabled_; }
  bool card_metadata_available() { return card_metadata_available_; }
  bool card_has_linked_virtual_card() { return card_has_linked_virtual_card_; }
  bool card_metadata_shown() {
    return (card_metadata_available_ &&
            (card_product_name_enabled_ || card_art_image_enabled_)) ||
           card_has_linked_virtual_card_;
  }

  FormData form() { return form_; }

  void SetUp() override {
    SetUpHelper();
    // Set up the form data. Reset form action to skip the IsFormMixedContent
    // check.
    form_ =
        GetAndAddSeenForm({.description_for_logging = "CardMetadata",
                           .fields = {{.role = CREDIT_CARD_NAME_FULL},
                                      {.role = CREDIT_CARD_NUMBER},
                                      {.role = CREDIT_CARD_EXP_MONTH},
                                      {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR}},
                           .action = ""});

    // Add 2 masked server cards.
    CreditCard card_without_metadata =
        test::GetRandomCreditCard(CreditCard::MASKED_SERVER_CARD);
    card_without_metadata.set_guid(kCardWithoutMetadataId);
    CreditCard card_with_metadata =
        test::GetRandomCreditCard(CreditCard::MASKED_SERVER_CARD);
    card_with_metadata.set_guid(kCardWithMetadataId);
    card_with_metadata.set_issuer_id("capitalone");
    // Set card_with_metadata as the virtual card.
    if (card_has_linked_virtual_card()) {
      card_with_metadata.set_virtual_card_enrollment_state(
          CreditCard::VirtualCardEnrollmentState::ENROLLED);
      card_with_metadata.set_card_art_url(
          GURL("https://www.example.com/cardart.png"));
    }
    // Set metadata to card_with_metadata.
    if (card_metadata_available()) {
      card_with_metadata.set_product_description(u"card_description");
      card_with_metadata.set_card_art_url(
          GURL("https://www.example.com/cardart.png"));
    }
    personal_data().AddServerCreditCard(card_without_metadata);
    personal_data().AddServerCreditCard(card_with_metadata);
    personal_data().Refresh();
  }

  void TearDown() override { TearDownHelper(); }

 private:
  const bool card_product_name_enabled_;
  const bool card_art_image_enabled_;
  const bool card_metadata_available_;
  const bool card_has_linked_virtual_card_;
  base::test::ScopedFeatureList feature_list_card_product_name_;
  base::test::ScopedFeatureList feature_list_card_art_image_;
  FormData form_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CardMetadataMetricsTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

// Test to ensure that the FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SHOWN is
// correctly logged when any card in the suggestion has metadata.
TEST_P(CardMetadataMetricsTest, LogCardMetadataShownMetrics) {
  base::HistogramTester histogram_tester;

  // Simulate activating the autofill popup for the credit card field.
  autofill_manager().OnAskForValuesToFillTest(form(), form().fields.back());
  autofill_manager().DidShowSuggestions(/*has_autofill_suggestions=*/true,
                                        form(), form().fields.back());

  // Verify that if metadata is shown for any of the cards, it is logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
                     Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SHOWN,
                            card_metadata_shown())));
}

// Test to ensure that the FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SELECTED is
// not logged if a card without metadata is selected.
TEST_P(CardMetadataMetricsTest,
       LogCardMetadataSelectedMetrics_CardWithoutMetadataSelected) {
  base::HistogramTester histogram_tester;

  // Simulate selecting card_without_metadata.
  autofill_manager().OnAskForValuesToFillTest(form(), form().fields.back());
  autofill_manager().DidShowSuggestions(/*has_autofill_suggestions=*/true,
                                        form(), form().fields.back());
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, form(), form().fields.back(),
      MakeFrontendId({.credit_card_id = kCardWithoutMetadataId}));

  // Verify that if metadata is shown, metrics for 'card with metadata shown' is
  // logged, but metrics for 'card with metadata selected' is not logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SHOWN,
                 card_metadata_shown()),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SELECTED, 0)));
}

// Test to ensure that the FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SELECTED is
// logged if a card with metadata is selected.
TEST_P(CardMetadataMetricsTest,
       LogCardMetadataSelectedMetrics_CardWithMetadataSelected) {
  base::HistogramTester histogram_tester;

  // Simulate selecting card_with_metadata.
  autofill_manager().OnAskForValuesToFillTest(form(), form().fields.back());
  autofill_manager().DidShowSuggestions(/*has_autofill_suggestions=*/true,
                                        form(), form().fields.back());
  if (card_has_linked_virtual_card()) {
    autofill_manager().FillOrPreviewVirtualCardInformation(
        mojom::RendererFormDataAction::kFill, kCardWithMetadataId, form(),
        form().fields.back());
  } else {
    autofill_manager().FillOrPreviewForm(
        mojom::RendererFormDataAction::kFill, form(), form().fields.back(),
        MakeFrontendId({.credit_card_id = kCardWithMetadataId}));
  }

  // Verify that if metadata is shown, metrics for both 'card with metadata
  // shown' and 'card with metadata selected' is logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
                     Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SHOWN,
                            card_metadata_shown()),
                     Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED,
                            !card_has_linked_virtual_card()),
                     Bucket(FORM_EVENT_VIRTUAL_CARD_SUGGESTION_SELECTED,
                            card_has_linked_virtual_card()),
                     Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SELECTED,
                            card_metadata_shown())));
}

// Test to ensure that we log card metadata related metrics only when card
// metadata is available.
TEST_P(CardMetadataMetricsTest, LogCardMetadataLatencyMetrics) {
  base::TimeTicks now = AutofillTickClock::NowTicks();
  TestAutofillTickClock test_clock;
  test_clock.SetNowTicks(now);
  base::HistogramTester histogram_tester;

  // Simulate activating the autofill popup for the credit card field.
  autofill_manager().OnAskForValuesToFillTest(form(), form().fields.back());
  autofill_manager().DidShowSuggestions(/*has_autofill_suggestions=*/true,
                                        form(), form().fields.back());
  test_clock.SetNowTicks(now + base::Seconds(2));
  autofill_manager().FillOrPreviewForm(
      mojom::RendererFormDataAction::kFill, form(), form().fields.front(),
      MakeFrontendId({.credit_card_id = kTestMaskedCardId}));

  std::string latency_histogram_prefix =
      "Autofill.CreditCard.SelectionLatencySinceShown.";

  std::string latency_histogram_suffix;
  // Card product name is shown when card_metadata_available() and
  // card_product_name_enabled() both return true.
  // Card art image is shown when 1. card_has_linked_virtual_card() or
  // 2. card_metadata_available() and card_art_image_enabled() both return true.
  if (card_metadata_available()) {
    if (card_product_name_enabled() &&
        (card_art_image_enabled() || card_has_linked_virtual_card())) {
      latency_histogram_suffix =
          autofill_metrics::kProductNameAndArtImageBothShownSuffix;
    } else if (card_product_name_enabled()) {
      latency_histogram_suffix = autofill_metrics::kProductNameShownOnlySuffix;
    } else if (card_art_image_enabled() || card_has_linked_virtual_card()) {
      latency_histogram_suffix = autofill_metrics::kArtImageShownOnlySuffix;
    } else {
      latency_histogram_suffix =
          autofill_metrics::kProductNameAndArtImageNotShownSuffix;
    }
  } else if (card_has_linked_virtual_card()) {
    latency_histogram_suffix = autofill_metrics::kArtImageShownOnlySuffix;
  } else {
    latency_histogram_suffix =
        autofill_metrics::kProductNameAndArtImageNotShownSuffix;
  }
  histogram_tester.ExpectUniqueSample(
      latency_histogram_prefix + latency_histogram_suffix, 2000, 1);
  histogram_tester.ExpectUniqueSample(
      latency_histogram_prefix + "CardWithIssuerId." +
          latency_histogram_suffix + ".CapitalOne",
      2000, 1);
}

}  // namespace autofill::autofill_metrics
