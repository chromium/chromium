// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_form_fill_router.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/mock_merchant_promo_code_manager.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::SaveArg;

namespace autofill {

using FieldPrediction =
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction;

namespace {
class MockSuggestionsHandler
    : public SingleFieldFormFiller::SuggestionsHandler {
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

class SingleFieldFormFillRouterTest : public testing::Test {
 protected:
  SingleFieldFormFillRouterTest() {
    prefs_ = test::PrefServiceForTesting();

    // Mock such that we don't trigger the cleanup.
    prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                       CHROME_VERSION_MAJOR);
    personal_data_manager_ = std::make_unique<TestPersonalDataManager>();
    web_data_service_ = base::MakeRefCounted<MockAutofillWebDataService>();
    autocomplete_history_manager_ =
        std::make_unique<MockAutocompleteHistoryManager>();
    autocomplete_history_manager_->Init(web_data_service_, prefs_.get(), false);
    merchant_promo_code_manager_ =
        std::make_unique<MockMerchantPromoCodeManager>();
    merchant_promo_code_manager_->Init(personal_data_manager_.get(),
                                       /*is_off_the_record=*/false);
    single_field_form_fill_router_ =
        std::make_unique<SingleFieldFormFillRouter>(
            autocomplete_history_manager_.get(),
            merchant_promo_code_manager_.get());
    test::CreateTestFormField(/*label=*/"", "Some Field Name", "SomePrefix",
                              "SomeType", &test_field_);
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<SingleFieldFormFillRouter> single_field_form_fill_router_;
  std::unique_ptr<TestPersonalDataManager> personal_data_manager_;
  scoped_refptr<MockAutofillWebDataService> web_data_service_;
  std::unique_ptr<MockAutocompleteHistoryManager> autocomplete_history_manager_;
  std::unique_ptr<MockMerchantPromoCodeManager> merchant_promo_code_manager_;
  std::unique_ptr<PrefService> prefs_;
  FormFieldData test_field_;
};

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnGetSingleFieldSuggestions call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToAutocompleteHistoryManager_OnGetSingleFieldSuggestions) {
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  EXPECT_CALL(*autocomplete_history_manager_, OnGetSingleFieldSuggestions);

  single_field_form_fill_router_->OnGetSingleFieldSuggestions(
      /*query_id=*/2, /*is_autocomplete_enabled=*/true,
      /*autoselect_first_suggestion=*/false, test_field_,
      suggestions_handler->GetWeakPtr(), SuggestionsContext());
}

// Ensure that the router routes to all SingleFieldFormFillers for this
// OnWillSubmitForm call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToAllSingleFieldFormFillers_OnWillSubmitForm) {
  FormData form_data;
  std::vector<FormFieldData> fields;
  size_t number_of_fields_for_testing = 3;
  for (size_t i = 0; i < number_of_fields_for_testing; i++) {
    fields.emplace_back();
  }

#if !BUILDFLAG(IS_IOS)
  for (size_t i = 0; i < number_of_fields_for_testing; i++) {
    fields.emplace_back();
  }
#endif  // !BUILDFLAG(IS_IOS)

  form_data.fields = fields;
  FormStructure form_structure{form_data};

  // Set the first |number_of_fields_for_testing| fields to be autocomplete
  // fields.
  for (size_t i = 0; i < number_of_fields_for_testing; i++) {
    form_structure.set_server_field_type_for_testing(i, UNKNOWN_TYPE);
  }

#if !BUILDFLAG(IS_IOS)
  // Set the next |number_of_fields_for_testing| fields to be merchant promo
  // code fields.
  for (size_t i = number_of_fields_for_testing;
       i < number_of_fields_for_testing * 2; i++) {
    form_structure.set_server_field_type_for_testing(i, MERCHANT_PROMO_CODE);
  }
#endif  // !BUILDFLAG(IS_IOS)

  std::vector<FormFieldData> submitted_autocomplete_fields;
  bool autocomplete_fields_is_autocomplete_enabled = false;
  EXPECT_CALL(*autocomplete_history_manager_, OnWillSubmitFormWithFields(_, _))
      .WillOnce(
          (DoAll(SaveArg<0>(&submitted_autocomplete_fields),
                 SaveArg<1>(&autocomplete_fields_is_autocomplete_enabled))));

#if !BUILDFLAG(IS_IOS)
  std::vector<FormFieldData> submitted_merchant_promo_code_fields;
  bool merchant_promo_code_fields_is_autocomplete_enabled = false;
  EXPECT_CALL(*merchant_promo_code_manager_, OnWillSubmitFormWithFields(_, _))
      .WillOnce((DoAll(
          SaveArg<0>(&submitted_merchant_promo_code_fields),
          SaveArg<1>(&merchant_promo_code_fields_is_autocomplete_enabled))));
#endif  // !BUILDFLAG(IS_IOS)

  single_field_form_fill_router_->OnWillSubmitForm(
      form_data, &form_structure, /*is_autocomplete_enabled=*/true);

  EXPECT_TRUE(submitted_autocomplete_fields.size() ==
              number_of_fields_for_testing);
  EXPECT_TRUE(autocomplete_fields_is_autocomplete_enabled);

#if !BUILDFLAG(IS_IOS)
  EXPECT_TRUE(submitted_merchant_promo_code_fields.size() ==
              number_of_fields_for_testing);
  EXPECT_TRUE(merchant_promo_code_fields_is_autocomplete_enabled);
#endif  // !BUILDFLAG(IS_IOS)
}

// Ensure that the router routes to SingleFieldFormFillers for this
// CancelPendingQueries call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToAllSingleFieldFormFillers_CancelPendingQueries) {
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  EXPECT_CALL(*autocomplete_history_manager_, CancelPendingQueries);

#if !BUILDFLAG(IS_IOS)
  EXPECT_CALL(*merchant_promo_code_manager_, CancelPendingQueries);
#endif  // !BUILDFLAG(IS_IOS)

  single_field_form_fill_router_->CancelPendingQueries(
      suggestions_handler.get());
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnRemoveCurrentSingleFieldSuggestion call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToAutocompleteHistoryManager_OnRemoveCurrentSingleFieldSuggestion) {
  EXPECT_CALL(*autocomplete_history_manager_,
              OnRemoveCurrentSingleFieldSuggestion);

  single_field_form_fill_router_->OnRemoveCurrentSingleFieldSuggestion(
      /*field_name=*/u"Field Name", /*value=*/u"Value",
      POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY);
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnSingleFieldSuggestionSelected call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToAutocompleteHistoryManager_OnSingleFieldSuggestionSelected) {
  EXPECT_CALL(*autocomplete_history_manager_, OnSingleFieldSuggestionSelected);

  single_field_form_fill_router_->OnSingleFieldSuggestionSelected(
      /*value=*/u"Value", POPUP_ITEM_ID_AUTOCOMPLETE_ENTRY);
}

#if !BUILDFLAG(IS_IOS)
// Ensure that the router routes to MerchantPromoCodeManager for this
// OnGetSingleFieldSuggestions call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToMerchantPromoCodeManager_OnGetSingleFieldSuggestions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kAutofillFillMerchantPromoCodeFields,
                                 features::kAutofillServerTypeTakesPrecedence},
                                {});
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  EXPECT_CALL(*merchant_promo_code_manager_, OnGetSingleFieldSuggestions);
  std::vector<FieldPrediction> merchant_promo_code_field_predictions;
  FieldPrediction merchant_promo_code_field_prediction;
  merchant_promo_code_field_prediction.set_type(MERCHANT_PROMO_CODE);
  merchant_promo_code_field_predictions.push_back(
      merchant_promo_code_field_prediction);
  SuggestionsContext context;
  AutofillField autofill_field;
  autofill_field.set_server_predictions(
      std::move(merchant_promo_code_field_predictions));
  context.focused_field = &autofill_field;
  single_field_form_fill_router_->OnGetSingleFieldSuggestions(
      /*query_id=*/2, /*is_autocomplete_enabled=*/true,
      /*autoselect_first_suggestion=*/false, test_field_,
      suggestions_handler->GetWeakPtr(), context);
}

// Ensure that the router routes to MerchantPromoCodeManager for this
// OnRemoveCurrentSingleFieldSuggestion call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToMerchantPromoCodeManager_OnRemoveCurrentSingleFieldSuggestion) {
  EXPECT_CALL(*merchant_promo_code_manager_,
              OnRemoveCurrentSingleFieldSuggestion);

  single_field_form_fill_router_->OnRemoveCurrentSingleFieldSuggestion(
      /*field_name=*/u"Field Name", /*value=*/u"Value",
      POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY);
}

// Ensure that the router routes to MerchantPromoCodeManager for this
// OnSingleFieldSuggestionSelected call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToMerchantPromoCodeManager_OnSingleFieldSuggestionSelected) {
  EXPECT_CALL(*merchant_promo_code_manager_, OnSingleFieldSuggestionSelected);

  single_field_form_fill_router_->OnSingleFieldSuggestionSelected(
      /*value=*/u"Value", POPUP_ITEM_ID_MERCHANT_PROMO_CODE_ENTRY);
}
#endif  // !BUILDFLAG(IS_IOS)

}  // namespace autofill
