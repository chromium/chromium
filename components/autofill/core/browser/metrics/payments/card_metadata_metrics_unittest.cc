// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/metrics/payments/card_metadata_metrics.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_test_base.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/test_autofill_tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::base::Bucket;

namespace autofill::autofill_metrics {

namespace {

constexpr char kCardGuid[] = "10000000-0000-0000-0000-000000000001";

}  // namespace

// Params:
// 1. Whether card metadata is available.
// 2. Whether card has a static card art image (instead of the rich card art
// from metadata).
// 3. Whether a larger-sized card art image is used.
// 4. Unique identifiers for the issuer of the card.
class CardMetadataFormEventMetricsTest
    : public AutofillMetricsBaseTest,
      public testing::Test,
      public testing::WithParamInterface<
          std::tuple<bool, bool, bool, std::string>> {
 public:
  CardMetadataFormEventMetricsTest() = default;
  ~CardMetadataFormEventMetricsTest() override = default;

  bool registered_card_issuer_available() { return issuer_id() != "Dummy"; }
  bool card_metadata_available() const { return std::get<0>(GetParam()); }
  bool card_has_static_art_image() const { return std::get<1>(GetParam()); }
  bool new_card_art_and_network_images_used() const {
    return std::get<2>(GetParam());
  }
  const std::string& issuer_id() const { return std::get<3>(GetParam()); }

  FormData form() { return form_; }
  const CreditCard& card() const { return card_; }

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

    // Add a masked server card.
    card_ = test::WithCvc(test::GetMaskedServerCard());
    card_.set_guid(kCardGuid);
    card_.set_issuer_id(issuer_id());
    if (issuer_id() == kCapitalOneCardIssuerId && card_has_static_art_image()) {
      if (new_card_art_and_network_images_used()) {
        card_.set_card_art_url(GURL(kCapitalOneLargeCardArtUrl));
      } else {
        card_.set_card_art_url(GURL(kCapitalOneCardArtUrl));
      }
    }
    // Set metadata to card. The `card_art_url` will be overriden with rich card
    // art url regarless of `card_has_static_art_image()` in the test set-up,
    // because rich card art, if available, is preferred by Payments server and
    // will be sent to the client .
    if (card_metadata_available()) {
      card_.set_product_description(u"card_description");
      card_.set_card_art_url(GURL("https://www.example.com/cardart.png"));
    }
    personal_data().AddServerCreditCard(card_);
    personal_data().Refresh();
  }

  void TearDown() override { TearDownHelper(); }

  std::string GetHistogramName(std::string_view event) {
    return base::StrCat({"Autofill.CreditCard.",
                         GetCardIssuerIdSuffix(issuer_id()) != ""
                             ? GetCardIssuerIdSuffix(issuer_id())
                             : issuer_id(),
                         event});
  }

 private:
  CreditCard card_;
  FormData form_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CardMetadataFormEventMetricsTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Values("amex",
                                                          "anz",
                                                          "capitalone",
                                                          "chase",
                                                          "citi",
                                                          "discover",
                                                          "lloyds",
                                                          "marqeta",
                                                          "nab",
                                                          "natwest",
                                                          "Dummy")));

// Test metadata shown metrics are correctly logged.
TEST_P(CardMetadataFormEventMetricsTest, LogShownMetrics) {
  base::HistogramTester histogram_tester;

  // Simulate activating the autofill popup for the credit card field.
  autofill_manager().OnAskForValuesToFillTest(form(), form().fields.back());
  DidShowAutofillSuggestions(form(), /*field_index=*/form().fields.size() - 1,
                             PopupItemId::kCreditCardEntry);

  // Verify that:
  // 1. if the card suggestion shown had metadata,
  // `FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SHOWN` is logged as many times as
  // the suggestions are shown, and
  // `FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SHOWN_ONCE` is logged only once.
  // 2.  if the card suggestion shown did not have metadata,
  // `FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SHOWN` is logged as many times
  // as the suggestions are shown, and
  // `FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SHOWN_ONCE` is logged only
  // once.
  // 3. if the card suggestion shown had a registered issuer id, two histograms
  // are logged which tell if the card from the issuer had metadata.
  // 4. For cards with issuer ids that are not registered, no issuer-specific
  // metadata metrics are logged.
  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
              BucketsInclude(
                  Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 1),
                  Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SHOWN,
                         card_metadata_available() ? 1 : 0),
                  Bucket(FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SHOWN,
                         card_metadata_available() ? 0 : 1),
                  Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SHOWN_ONCE,
                         card_metadata_available() ? 1 : 0),
                  Bucket(FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SHOWN_ONCE,
                         card_metadata_available() ? 0 : 1)));

  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".ShownWithMetadata"), card_metadata_available(),
      registered_card_issuer_available() ? 1 : 0);
  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".ShownWithMetadataOnce"), card_metadata_available(),
      registered_card_issuer_available() ? 1 : 0);
  histogram_tester.ExpectUniqueSample("Autofill.CreditCard..ShownWithMetadata",
                                      card_metadata_available(), 0);

  // Show the popup again.
  autofill_manager().OnAskForValuesToFillTest(form(), form().fields.back());
  DidShowAutofillSuggestions(form(), /*field_index=*/form().fields.size() - 1,
                             PopupItemId::kCreditCardEntry);

  EXPECT_THAT(histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
              BucketsInclude(
                  Bucket(FORM_EVENT_SUGGESTIONS_SHOWN, 2),
                  Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SHOWN,
                         card_metadata_available() ? 2 : 0),
                  Bucket(FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SHOWN,
                         card_metadata_available() ? 0 : 2),
                  Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SHOWN_ONCE,
                         card_metadata_available() ? 1 : 0),
                  Bucket(FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SHOWN_ONCE,
                         card_metadata_available() ? 0 : 1)));
  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".ShownWithMetadata"), card_metadata_available(),
      registered_card_issuer_available() ? 2 : 0);
  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".ShownWithMetadataOnce"), card_metadata_available(),
      registered_card_issuer_available() ? 1 : 0);
}

// Test metadata selected metrics are correctly logged.
TEST_P(CardMetadataFormEventMetricsTest, LogSelectedMetrics) {
  // Add a second card which won't be selected but will be logged in
  // Autofill.CreditCard.Amex.SelectedWithIssuerMetadataPresentOnce.
  CreditCard card2 = test::GetMaskedServerCard2();
  card2.set_guid(kTestMaskedCardId);
  card2.set_issuer_id("amex");
  if (card_metadata_available()) {
    card2.set_product_description(u"product description");
    card2.set_card_art_url(GURL("https://www.example.com/cardarturl.png"));
  }
  personal_data().AddServerCreditCard(card2);

  base::HistogramTester histogram_tester;

  // Simulate selecting the card.
  autofill_manager().OnAskForValuesToFillTest(form(), form().fields.back());
  DidShowAutofillSuggestions(form(), /*field_index=*/form().fields.size() - 1,
                             PopupItemId::kCreditCardEntry);
  autofill_manager().FillOrPreviewCreditCardForm(
      mojom::ActionPersistence::kFill, form(), form().fields.back(),
      *personal_data().GetCreditCardByGUID(kCardGuid),
      {.trigger_source = AutofillTriggerSource::kPopup});

  // Verify that:
  // 1. if the card suggestion selected had metadata,
  // `FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SELECTED` is logged as many times
  // as the suggestions are selected, and
  // `FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SELECTED_ONCE` is logged only
  // once.
  // 2. if the card suggestion selected did not have metadata,
  // `FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SELECTED` is logged as many
  // times as the suggestions are selected, and
  // `FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SELECTED_ONCE` is logged only
  // once.
  // 3. if the card suggestion selected had a registered issuer id, two
  // histograms are logged which tell if the card from the issuer had metadata.
  // 4. For cards with issuer ids that are not registered, no issuer-specific
  // metadata metrics are logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SELECTED,
                 card_metadata_available() ? 1 : 0),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SELECTED,
                 card_metadata_available() ? 0 : 1),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SELECTED_ONCE,
                 card_metadata_available() ? 1 : 0),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SELECTED_ONCE,
                 card_metadata_available() ? 0 : 1)));
  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".SelectedWithMetadata"), card_metadata_available(),
      registered_card_issuer_available() ? 1 : 0);
  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".SelectedWithMetadataOnce"), card_metadata_available(),
      registered_card_issuer_available() ? 1 : 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCard..SelectedWithMetadata", card_metadata_available(),
      0);

  // Select the suggestion again.
  autofill_manager().FillOrPreviewCreditCardForm(
      mojom::ActionPersistence::kFill, form(), form().fields.back(),
      *personal_data().GetCreditCardByGUID(kCardGuid),
      {.trigger_source = AutofillTriggerSource::kPopup});

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 2),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SELECTED,
                 card_metadata_available() ? 2 : 0),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SELECTED,
                 card_metadata_available() ? 0 : 2),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SELECTED_ONCE,
                 card_metadata_available() ? 1 : 0),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SELECTED_ONCE,
                 card_metadata_available() ? 0 : 1)));
  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".SelectedWithMetadata"), card_metadata_available(),
      registered_card_issuer_available() ? 2 : 0);
  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".SelectedWithMetadataOnce"), card_metadata_available(),
      registered_card_issuer_available() ? 1 : 0);
  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".SelectedWithIssuerMetadataPresentOnce"), true,
      card_metadata_available() && registered_card_issuer_available() ? 1 : 0);

  // Only test non-Amex because for Amex case it will log true in
  // SelectedWithIssuerMetadataPresentOnce histogram.
  if (issuer_id() != "amex") {
    histogram_tester.ExpectUniqueSample(
        "Autofill.CreditCard.Amex.SelectedWithIssuerMetadataPresentOnce", false,
        card_metadata_available() && registered_card_issuer_available() ? 1
                                                                        : 0);
  }
}

// Test metadata filled metrics are correctly logged.
TEST_P(CardMetadataFormEventMetricsTest, LogFilledMetrics) {
  base::HistogramTester histogram_tester;

  // Simulate filling the card.
  autofill_manager().FillOrPreviewCreditCardForm(
      mojom::ActionPersistence::kFill, form(), form().fields.back(),
      *personal_data().GetCreditCardByGUID(kCardGuid),
      {.trigger_source = AutofillTriggerSource::kPopup});
  test_api(autofill_manager())
      .OnCreditCardFetched(CreditCardFetchResult::kSuccess, &card());

  // Verify that:
  // 1. if the card suggestion filled had metadata,
  // `FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_FILLED` is logged as many times
  // as the suggestions are filled, and
  // `FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_FILLED_ONCE` is logged only
  // once.
  // 2. if the card suggestion filled did not have metadata,
  // `FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_FILLED` is logged as many
  // times as the suggestions are filled, and
  // `FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_FILLED_ONCE` is logged only
  // once.
  // 3. if the card suggestion filled had a registered issuer id, two histograms
  // are logged which tell if the card from the issuer had metadata.
  // 4. For cards with issuer ids that are not registered, no issuer-specific
  // metadata metrics are logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_FILLED,
                 card_metadata_available() ? 1 : 0),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_FILLED,
                 card_metadata_available() ? 0 : 1),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_FILLED_ONCE,
                 card_metadata_available() ? 1 : 0),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_FILLED_ONCE,
                 card_metadata_available() ? 0 : 1)));
  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".FilledWithMetadata"), card_metadata_available(),
      registered_card_issuer_available() ? 1 : 0);
  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".FilledWithMetadataOnce"), card_metadata_available(),
      registered_card_issuer_available() ? 1 : 0);
  histogram_tester.ExpectUniqueSample("Autofill.CreditCard..FilledWithMetadata",
                                      card_metadata_available(), 0);

  // Fill the suggestion again.
  test_api(autofill_manager())
      .OnCreditCardFetched(CreditCardFetchResult::kSuccess, &card());

  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 2),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_FILLED,
                 card_metadata_available() ? 2 : 0),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_FILLED,
                 card_metadata_available() ? 0 : 2),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_FILLED_ONCE,
                 card_metadata_available() ? 1 : 0),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_FILLED_ONCE,
                 card_metadata_available() ? 0 : 1)));
  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".FilledWithMetadata"), card_metadata_available(),
      registered_card_issuer_available() ? 2 : 0);
  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".FilledWithMetadataOnce"), card_metadata_available(),
      registered_card_issuer_available() ? 1 : 0);
}

// Test metadata will submit and submitted metrics are correctly logged.
TEST_P(CardMetadataFormEventMetricsTest, LogSubmitMetrics) {
  base::HistogramTester histogram_tester;

  // Simulate filling and then submitting the card.
  autofill_manager().OnAskForValuesToFillTest(form(), form().fields.back());
  autofill_manager().FillOrPreviewCreditCardForm(
      mojom::ActionPersistence::kFill, form(), form().fields.back(),
      *personal_data().GetCreditCardByGUID(kCardGuid),
      {.trigger_source = AutofillTriggerSource::kPopup});
  test_api(autofill_manager())
      .OnCreditCardFetched(CreditCardFetchResult::kSuccess, &card());
  SubmitForm(form());

  // Verify that:
  // 1. if the form was submitted after a card suggestion with metadata was
  // filled, `FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_WILL_SUBMIT_ONCE` and
  // `FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SUBMITTED_ONCE` are logged.
  // 2. if the form was submitted after a card suggestion without metadata was
  // filled, `FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_WILL_SUBMIT_ONCE` and
  // `FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SUBMITTED_ONCE` are logged.
  // 3. if the form was submitted after a card suggestion with a registered
  // issuer id was filled, two histograms are logged which tell if the card from
  // the issuer had metadata.
  // 4. For cards with issuer ids that are not registered, no issuer-specific
  // metadata metrics are logged.
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.FormEvents.CreditCard"),
      BucketsInclude(
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE, 1),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_WILL_SUBMIT_ONCE,
                 card_metadata_available() ? 1 : 0),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_WILL_SUBMIT_ONCE,
                 card_metadata_available() ? 0 : 1),
          Bucket(FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE, 1),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITH_METADATA_SUBMITTED_ONCE,
                 card_metadata_available() ? 1 : 0),
          Bucket(FORM_EVENT_CARD_SUGGESTION_WITHOUT_METADATA_SUBMITTED_ONCE,
                 card_metadata_available() ? 0 : 1)));
  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".WillSubmitWithMetadataOnce"),
      card_metadata_available(), registered_card_issuer_available() ? 1 : 0);
  histogram_tester.ExpectUniqueSample(
      GetHistogramName(".SubmittedWithMetadataOnce"), card_metadata_available(),
      registered_card_issuer_available() ? 1 : 0);
  histogram_tester.ExpectUniqueSample(
      "Autofill.CreditCard..SubmittedWithMetadata", card_metadata_available(),
      0);
}

// Params:
// 1) Whether card product name feature flag is enabled.
// 2) whether card art image feature flag is enabled.
// 3) Whether card metadata (both product name and card art image) are provided.
// 4) Whether the card has linked virtual card (only card art is provided).
class CardMetadataLatencyMetricsTest
    : public AutofillMetricsBaseTest,
      public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, bool, bool, bool>> {
 public:
  CardMetadataLatencyMetricsTest() = default;
  ~CardMetadataLatencyMetricsTest() override = default;

  bool card_product_name_enabled() { return std::get<0>(GetParam()); }
  bool card_art_image_enabled() { return std::get<1>(GetParam()); }
  bool card_metadata_available() { return std::get<2>(GetParam()); }
  bool card_has_static_art_image() { return std::get<3>(GetParam()); }

  FormData form() { return form_; }

  void SetUp() override {
    SetUpHelper();
    feature_list_card_product_name_.InitWithFeatureState(
        features::kAutofillEnableCardProductName, card_product_name_enabled());
    feature_list_card_art_image_.InitWithFeatureState(
        features::kAutofillEnableCardArtImage, card_art_image_enabled());
    // Set up the form data. Reset form action to skip the IsFormMixedContent
    // check.
    form_ =
        GetAndAddSeenForm({.description_for_logging = "CardMetadata",
                           .fields = {{.role = CREDIT_CARD_NAME_FULL},
                                      {.role = CREDIT_CARD_NUMBER},
                                      {.role = CREDIT_CARD_EXP_MONTH},
                                      {.role = CREDIT_CARD_EXP_2_DIGIT_YEAR}},
                           .action = ""});

    CreditCard masked_server_card = test::GetMaskedServerCard();
    masked_server_card.set_guid(kTestMaskedCardId);
    masked_server_card.set_issuer_id(kCapitalOneCardIssuerId);
    if (card_has_static_art_image()) {
      masked_server_card.set_card_art_url(GURL(kCapitalOneCardArtUrl));
    }
    // If metadata is available, the `card_art_url` will be overriden with rich
    // card art url regarless of `card_has_static_art_image()` in the test
    // set-up, because rich card art, if available, is preferred by Payments
    // server and will be sent to the client.
    if (card_metadata_available()) {
      masked_server_card.set_product_description(u"card_description");
      masked_server_card.set_card_art_url(
          GURL("https://www.example.com/cardart.png"));
    }
    personal_data().AddServerCreditCard(masked_server_card);
    personal_data().Refresh();
  }

  void TearDown() override { TearDownHelper(); }

 private:
  base::test::ScopedFeatureList feature_list_card_product_name_;
  base::test::ScopedFeatureList feature_list_card_art_image_;
  FormData form_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         CardMetadataLatencyMetricsTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

// Test to ensure that we log card metadata related metrics only when card
// metadata is available.
TEST_P(CardMetadataLatencyMetricsTest, LogMetrics) {
  base::TimeTicks now = AutofillTickClock::NowTicks();
  TestAutofillTickClock test_clock;
  test_clock.SetNowTicks(now);
  base::HistogramTester histogram_tester;

  // Simulate activating the autofill popup for the credit card field.
  autofill_manager().OnAskForValuesToFillTest(form(), form().fields.back());
  DidShowAutofillSuggestions(form(), /*field_index=*/form().fields.size() - 1,
                             PopupItemId::kCreditCardEntry);
  test_clock.SetNowTicks(now + base::Seconds(2));
  autofill_manager().FillOrPreviewCreditCardForm(
      mojom::ActionPersistence::kFill, form(), form().fields.front(),
      *personal_data().GetCreditCardByGUID(kTestMaskedCardId),
      {.trigger_source = AutofillTriggerSource::kPopup});

  std::string latency_histogram_prefix =
      "Autofill.CreditCard.SelectionLatencySinceShown.";

  std::string latency_histogram_suffix;
  // Card product name is shown when card_metadata_available() and
  // card_product_name_enabled() both return true.
  // Card art image is shown when 1. card_has_linked_virtual_card() or
  // 2. card_metadata_available() and card_art_image_enabled() both return true.
  if (card_metadata_available()) {
    if (card_product_name_enabled() && card_art_image_enabled()) {
      latency_histogram_suffix =
          autofill_metrics::kProductNameAndArtImageBothShownSuffix;
    } else if (card_product_name_enabled()) {
      latency_histogram_suffix = autofill_metrics::kProductNameShownOnlySuffix;
    } else if (card_art_image_enabled()) {
      latency_histogram_suffix = autofill_metrics::kArtImageShownOnlySuffix;
    } else {
      latency_histogram_suffix =
          autofill_metrics::kProductNameAndArtImageNotShownSuffix;
    }
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
