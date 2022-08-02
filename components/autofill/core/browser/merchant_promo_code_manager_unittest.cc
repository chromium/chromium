// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/merchant_promo_code_manager.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/payments/offers_metrics.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/form_data.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using testing::Field;
using testing::UnorderedElementsAre;

namespace autofill {

namespace {

class MockSuggestionsHandler
    : public MerchantPromoCodeManager::SuggestionsHandler {
 public:
  MockSuggestionsHandler() = default;
  MockSuggestionsHandler(const MockSuggestionsHandler&) = delete;
  MockSuggestionsHandler& operator=(const MockSuggestionsHandler&) = delete;
  ~MockSuggestionsHandler() override = default;

  MOCK_METHOD(void,
              OnSuggestionsReturned,
              (int query_id,
               bool autoselect_first_suggestion,
               const std::vector<Suggestion>& suggestions),
              (override));

  base::WeakPtr<MockSuggestionsHandler> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockSuggestionsHandler> weak_ptr_factory_{this};
};
}  // namespace

class MerchantPromoCodeManagerTest : public testing::Test {
 protected:
  MerchantPromoCodeManagerTest() {
    personal_data_manager_ = std::make_unique<TestPersonalDataManager>();
    merchant_promo_code_manager_ = std::make_unique<MerchantPromoCodeManager>();
    merchant_promo_code_manager_->Init(personal_data_manager_.get(),
                                       /*is_off_the_record=*/false);
    test::CreateTestFormField(/*label=*/"", "Some Field Name", "SomePrefix",
                              "Some Type", &test_field_);
  }

  // Sets up the TestPersonalDataManager with a promo code offer for the given
  // |origin|, and sets the offer details url of the offer to
  // |offer_details_url|. Returns the promo code inserted in case the test wants
  // to match it against returned suggestions.
  std::string SetUpPromoCodeOffer(std::string origin,
                                  const GURL& offer_details_url) {
    personal_data_manager_.get()->SetAutofillWalletImportEnabled(true);
    AutofillOfferData testPromoCodeOfferData =
        test::GetPromoCodeOfferData(GURL(origin));
    testPromoCodeOfferData.SetOfferDetailsUrl(offer_details_url);
    personal_data_manager_.get()->AddOfferDataForTest(
        std::make_unique<AutofillOfferData>(testPromoCodeOfferData));
    return testPromoCodeOfferData.GetPromoCode();
  }

  std::unique_ptr<MerchantPromoCodeManager> merchant_promo_code_manager_;
  std::unique_ptr<TestPersonalDataManager> personal_data_manager_;
  FormFieldData test_field_;
};

TEST_F(MerchantPromoCodeManagerTest, ShowsPromoCodeSuggestions) {
  base::HistogramTester histogram_tester;
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  int test_query_id = 2;
  bool autoselect_first_suggestion = false;
  bool is_autocomplete_enabled = true;
  std::string last_committed_origin_url = "https://www.example.com";
  FormData form_data;
  form_data.main_frame_origin =
      url::Origin::Create(GURL(last_committed_origin_url));
  FormStructure form_structure{form_data};
  SuggestionsContext context;
  context.form_structure = &form_structure;
  std::string promo_code = SetUpPromoCodeOffer(
      last_committed_origin_url, GURL("https://offer-details-url.com/"));
  Suggestion promo_code_suggestion = Suggestion(base::ASCIIToUTF16(promo_code));
  Suggestion footer_suggestion = Suggestion(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_PROMO_CODE_SUGGESTIONS_FOOTER_TEXT));

  // Setting up mock to verify that the handler is returned a list of
  // promo-code-based suggestions and the promo code details line.
  EXPECT_CALL(
      *suggestions_handler.get(),
      OnSuggestionsReturned(
          test_query_id, autoselect_first_suggestion,
          UnorderedElementsAre(
              Field(&Suggestion::main_text, promo_code_suggestion.main_text),
              Field(&Suggestion::frontend_id, POPUP_ITEM_ID_SEPARATOR),
              Field(&Suggestion::main_text, footer_suggestion.main_text))))
      .Times(3);

  // Simulate request for suggestions.
  // Because all criteria are met, active promo code suggestions for the given
  // merchant site will be displayed instead of requesting Autocomplete
  // suggestions.
  merchant_promo_code_manager_->OnGetSingleFieldSuggestions(
      test_query_id, is_autocomplete_enabled, autoselect_first_suggestion,
      test_field_, suggestions_handler->GetWeakPtr(),
      /*context=*/context);

  // Trigger offers suggestions popup again to be able to test that we do not
  // log metrics twice for the same field.
  merchant_promo_code_manager_->OnGetSingleFieldSuggestions(
      test_query_id, is_autocomplete_enabled, autoselect_first_suggestion,
      test_field_, suggestions_handler->GetWeakPtr(),
      /*context=*/context);

  // Trigger offers suggestions popup again to be able to test that we log
  // metrics more than once if it is a different field.
  FormFieldData other_field;
  test::CreateTestFormField(/*label=*/"", "Some Other Name", "SomePrefix",
                            "Some Type", &other_field);
  merchant_promo_code_manager_->OnGetSingleFieldSuggestions(
      test_query_id, is_autocomplete_enabled, autoselect_first_suggestion,
      other_field, suggestions_handler->GetWeakPtr(),
      /*context=*/context);

  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.SuggestionsPopupShown",
      autofill_metrics::OffersSuggestionsPopupEvent::
          kOffersSuggestionsPopupShownOnce,
      2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.SuggestionsPopupShown",
      autofill_metrics::OffersSuggestionsPopupEvent::
          kOffersSuggestionsPopupShown,
      3);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionShownOnce, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionShown, 3);
}

TEST_F(MerchantPromoCodeManagerTest,
       DoesNotShowPromoCodeOffersForOffTheRecord) {
  base::HistogramTester histogram_tester;
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  std::string last_committed_origin_url = "https://www.example.com";
  std::string promo_code = SetUpPromoCodeOffer(
      last_committed_origin_url, GURL("https://offer-details-url.com/"));
  FormData form_data;
  form_data.main_frame_origin =
      url::Origin::Create(GURL(last_committed_origin_url));
  FormStructure form_structure{form_data};
  SuggestionsContext context;
  context.form_structure = &form_structure;
  merchant_promo_code_manager_->is_off_the_record_ = true;

  // Setting up mock to verify that suggestions returning is not triggered if
  // the user is off the record.
  EXPECT_CALL(*suggestions_handler, OnSuggestionsReturned).Times(0);

  // Simulate request for suggestions.
  merchant_promo_code_manager_->OnGetSingleFieldSuggestions(
      /*query_id=*/2, /*is_autocomplete_enabled=*/true,
      /*autoselect_first_suggestion=*/false, test_field_,
      suggestions_handler->GetWeakPtr(),
      /*context=*/context);

  // Ensure that no metrics were logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.SuggestionsPopupShown",
      autofill_metrics::OffersSuggestionsPopupEvent::
          kOffersSuggestionsPopupShownOnce,
      0);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.SuggestionsPopupShown",
      autofill_metrics::OffersSuggestionsPopupEvent::
          kOffersSuggestionsPopupShown,
      0);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionShownOnce, 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionShown, 0);
}

TEST_F(MerchantPromoCodeManagerTest,
       DoesNotShowPromoCodeOffersIfPersonalDataManagerDoesNotExist) {
  base::HistogramTester histogram_tester;
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  std::string last_committed_origin_url = "https://www.example.com";
  FormData form_data;
  form_data.main_frame_origin =
      url::Origin::Create(GURL(last_committed_origin_url));
  FormStructure form_structure{form_data};
  SuggestionsContext context;
  context.form_structure = &form_structure;
  merchant_promo_code_manager_->personal_data_manager_ = nullptr;

  // Setting up mock to verify that suggestions returning is not triggered if
  // personal data manager does not exist.
  EXPECT_CALL(*suggestions_handler, OnSuggestionsReturned).Times(0);

  // Simulate request for suggestions.
  merchant_promo_code_manager_->OnGetSingleFieldSuggestions(
      /*query_id=*/2, /*is_autocomplete_enabled=*/true,
      /*autoselect_first_suggestion=*/false, test_field_,
      suggestions_handler->GetWeakPtr(),
      /*context=*/context);

  // Ensure that no metrics were logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.SuggestionsPopupShown",
      autofill_metrics::OffersSuggestionsPopupEvent::
          kOffersSuggestionsPopupShownOnce,
      0);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.SuggestionsPopupShown",
      autofill_metrics::OffersSuggestionsPopupEvent::
          kOffersSuggestionsPopupShown,
      0);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionShownOnce, 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionShown, 0);
}

TEST_F(MerchantPromoCodeManagerTest, NoPromoCodeOffers) {
  base::HistogramTester histogram_tester;
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  std::string last_committed_origin_url = "https://www.example.com";
  personal_data_manager_.get()->SetAutofillWalletImportEnabled(true);
  FormData form_data;
  form_data.main_frame_origin =
      url::Origin::Create(GURL(last_committed_origin_url));
  FormStructure form_structure{form_data};
  SuggestionsContext context;
  context.form_structure = &form_structure;

  // Setting up mock to verify that suggestions returning is not triggered if
  // there are no promo code offers to suggest.
  EXPECT_CALL(*suggestions_handler, OnSuggestionsReturned).Times(0);

  // Simulate request for suggestions.
  merchant_promo_code_manager_->OnGetSingleFieldSuggestions(
      /*query_id=*/2, /*is_autocomplete_enabled=*/true,
      /*autoselect_first_suggestion=*/false, test_field_,
      suggestions_handler->GetWeakPtr(),
      /*context=*/context);

  // Ensure that no metrics were logged.
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.SuggestionsPopupShown",
      autofill_metrics::OffersSuggestionsPopupEvent::
          kOffersSuggestionsPopupShownOnce,
      0);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.SuggestionsPopupShown",
      autofill_metrics::OffersSuggestionsPopupEvent::
          kOffersSuggestionsPopupShown,
      0);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionShownOnce, 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionShown, 0);
}

TEST_F(MerchantPromoCodeManagerTest,
       OnSingleFieldSuggestion_GPayPromoCodeOfferSuggestion) {
  // Set up the test.
  base::HistogramTester histogram_tester;
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  int test_query_id = 2;
  std::u16string test_promo_code = u"test_promo_code";
  bool autoselect_first_suggestion = false;
  bool is_autocomplete_enabled = true;
  std::string last_committed_origin_url = "https://www.example.com";
  FormData form_data;
  form_data.main_frame_origin =
      url::Origin::Create(GURL(last_committed_origin_url));
  FormStructure form_structure{form_data};
  SuggestionsContext context;
  context.form_structure = &form_structure;
  SetUpPromoCodeOffer(last_committed_origin_url,
                      GURL("https://offer-details-url.com/"));

  // Check that non promo code frontend id's do not log as offer suggestion
  // selected.
  merchant_promo_code_manager_->OnSingleFieldSuggestionSelected(
      test_promo_code, POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionSelected, 0);

  // Simulate showing the promo code offers suggestions popup.
  merchant_promo_code_manager_->OnGetSingleFieldSuggestions(
      test_query_id, is_autocomplete_enabled, autoselect_first_suggestion,
      test_field_, suggestions_handler->GetWeakPtr(),
      /*context=*/context);

  // Simulate selecting a promo code offer suggestion.
  merchant_promo_code_manager_->OnSingleFieldSuggestionSelected(
      test_promo_code, POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY);

  // Check that the histograms logged correctly.
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionSelected, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionSelectedOnce,
      1);

  // Simulate showing the promo code offers suggestions popup.
  merchant_promo_code_manager_->OnGetSingleFieldSuggestions(
      test_query_id, is_autocomplete_enabled, autoselect_first_suggestion,
      test_field_, suggestions_handler->GetWeakPtr(),
      /*context=*/context);

  // Simulate selecting a promo code offer suggestion.
  merchant_promo_code_manager_->OnSingleFieldSuggestionSelected(
      test_promo_code, POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY);

  // Check that the histograms logged correctly.
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionSelected, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::kOfferSuggestionSelectedOnce,
      1);
}

TEST_F(MerchantPromoCodeManagerTest,
       OnSingleFieldSuggestion_GPayPromoCodeOfferFooter) {
  // Set up the test.
  base::HistogramTester histogram_tester;
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
  int test_query_id = 2;
  std::u16string test_promo_code = u"test_promo_code";
  bool autoselect_first_suggestion = false;
  bool is_autocomplete_enabled = true;
  std::string last_committed_origin_url = "https://www.example.com";
  FormData form_data;
  form_data.main_frame_origin =
      url::Origin::Create(GURL(last_committed_origin_url));
  FormStructure form_structure{form_data};
  SuggestionsContext context;
  context.form_structure = &form_structure;
  SetUpPromoCodeOffer(last_committed_origin_url,
                      GURL("https://offer-details-url.com/"));

  // Check that non promo code footer frontend id's do not log as offer
  // suggestions footer selected.
  merchant_promo_code_manager_->OnSingleFieldSuggestionSelected(
      test_promo_code, POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::
          kOfferSuggestionSeeOfferDetailsSelected,
      0);

  // Simulate showing the promo code offers suggestions popup.
  merchant_promo_code_manager_->OnGetSingleFieldSuggestions(
      test_query_id, is_autocomplete_enabled, autoselect_first_suggestion,
      test_field_, suggestions_handler->GetWeakPtr(),
      /*context=*/context);

  // Simulate selecting a promo code offer suggestion.
  merchant_promo_code_manager_->OnSingleFieldSuggestionSelected(
      test_promo_code, POPUP_ITEM_ID_SEE_PROMO_CODE_DETAILS);

  // Check that the histograms logged correctly.
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::
          kOfferSuggestionSeeOfferDetailsSelected,
      1);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::
          kOfferSuggestionSeeOfferDetailsSelectedOnce,
      1);

  // Simulate showing the promo code offers suggestions popup.
  merchant_promo_code_manager_->OnGetSingleFieldSuggestions(
      test_query_id, is_autocomplete_enabled, autoselect_first_suggestion,
      test_field_, suggestions_handler->GetWeakPtr(),
      /*context=*/context);

  // Simulate selecting a promo code offer suggestion.
  merchant_promo_code_manager_->OnSingleFieldSuggestionSelected(
      test_promo_code, POPUP_ITEM_ID_SEE_PROMO_CODE_DETAILS);

  // Check that the histograms logged correctly.
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::
          kOfferSuggestionSeeOfferDetailsSelected,
      2);
  histogram_tester.ExpectBucketCount(
      "Autofill.Offer.Suggestion.GPayPromoCodeOffer",
      autofill_metrics::OffersSuggestionsEvent::
          kOfferSuggestionSeeOfferDetailsSelectedOnce,
      1);
}

}  // namespace autofill
