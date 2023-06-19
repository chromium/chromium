// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_form_fill_router.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/mock_iban_manager.h"
#include "components/autofill/core/browser/mock_merchant_promo_code_manager.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/version_info/version_info.h"
#include "form_structure_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::SaveArg;

namespace autofill {

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
              (FieldGlobalId field_id,
               AutofillSuggestionTriggerSource trigger_source,
               const std::vector<Suggestion>& suggestions),
              (override));

  base::WeakPtr<MockSuggestionsHandler> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockSuggestionsHandler> weak_ptr_factory_{this};
};

FormStructureTestApi test_api(FormStructure* form_structure) {
  return FormStructureTestApi(form_structure);
}

}  // namespace

class SingleFieldFormFillRouterTest : public testing::Test {
 protected:
  SingleFieldFormFillRouterTest() {
    scoped_feature_list_async_parse_form_.InitWithFeatureState(
        features::kAutofillParseAsync, true);

    prefs_ = test::PrefServiceForTesting();

    // Mock such that we don't trigger the cleanup.
    prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                       CHROME_VERSION_MAJOR);
    personal_data_manager_ = std::make_unique<TestPersonalDataManager>();
    web_data_service_ = base::MakeRefCounted<MockAutofillWebDataService>();
    autocomplete_history_manager_ =
        std::make_unique<MockAutocompleteHistoryManager>();
    autocomplete_history_manager_->Init(web_data_service_, prefs_.get(), false);
    iban_manager_ =
        std::make_unique<MockIBANManager>(personal_data_manager_.get());
    merchant_promo_code_manager_ =
        std::make_unique<MockMerchantPromoCodeManager>();
    merchant_promo_code_manager_->Init(personal_data_manager_.get(),
                                       /*is_off_the_record=*/false);
    single_field_form_fill_router_ =
        std::make_unique<SingleFieldFormFillRouter>(
            autocomplete_history_manager_.get(), iban_manager_.get(),
            merchant_promo_code_manager_.get());
    test::CreateTestFormField(/*label=*/"", "Some Field Name", "SomePrefix",
                              "SomeType", &test_field_);
  }

  base::test::ScopedFeatureList scoped_feature_list_async_parse_form_;
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<SingleFieldFormFillRouter> single_field_form_fill_router_;
  std::unique_ptr<TestPersonalDataManager> personal_data_manager_;
  scoped_refptr<MockAutofillWebDataService> web_data_service_;
  std::unique_ptr<PrefService> prefs_;
  std::unique_ptr<MockAutocompleteHistoryManager> autocomplete_history_manager_;
  std::unique_ptr<MockIBANManager> iban_manager_;
  std::unique_ptr<MockMerchantPromoCodeManager> merchant_promo_code_manager_;
  FormFieldData test_field_;
};

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnGetSingleFieldSuggestions call if the field has autocomplete on.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToAutocompleteHistoryManager_OnGetSingleFieldSuggestions) {
  for (bool test_field_should_autocomplete : {true, false}) {
    SCOPED_TRACE(testing::Message() << "test_field_should_autocomplete = "
                                    << test_field_should_autocomplete);
    auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
    test_field_.should_autocomplete = test_field_should_autocomplete;

    // If `test_field_.should_autocomplete` is true, that means autocomplete is
    // turned on for the given test field and
    // AutocompleteHistoryManager::OnGetSingleFieldSuggestions() should return
    // true. If `test_field_.should_autocomplete` is false, then autocomplete is
    // turned off for the given test field and
    // AutocompleteHistoryManager::OnGetSingleFieldSuggestions() should return
    // false.
    EXPECT_CALL(*autocomplete_history_manager_, OnGetSingleFieldSuggestions)
        .Times(1)
        .WillOnce(testing::Return(test_field_.should_autocomplete));

    EXPECT_EQ(
        test_field_.should_autocomplete,
        single_field_form_fill_router_->OnGetSingleFieldSuggestions(
            AutofillSuggestionTriggerSource::kFormControlElementClicked,
            test_field_, autofill_client_, suggestions_handler->GetWeakPtr(),
            /*context=*/SuggestionsContext()));
  }
}

// Ensure that the router routes to all SingleFieldFormFillers for this
// OnWillSubmitForm call, and call OnWillSubmitFormWithFields
// if corresponding manager (e.g., IBANManager) presents.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToAllSingleFieldFormFillers_OnWillSubmitForm) {
  FormData form_data;
  size_t number_of_fields_for_testing = 3;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  form_data.fields.resize(3 * number_of_fields_for_testing);
#else
  form_data.fields.resize(2 * number_of_fields_for_testing);
#endif

  FormStructure form_structure{form_data};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  test_api(&form_structure)
      .SetFieldTypes({UNKNOWN_TYPE, UNKNOWN_TYPE, UNKNOWN_TYPE,
                      MERCHANT_PROMO_CODE, MERCHANT_PROMO_CODE,
                      MERCHANT_PROMO_CODE, IBAN_VALUE, IBAN_VALUE, IBAN_VALUE});
#else
  test_api(&form_structure)
      .SetFieldTypes({UNKNOWN_TYPE, UNKNOWN_TYPE, UNKNOWN_TYPE,
                      MERCHANT_PROMO_CODE, MERCHANT_PROMO_CODE,
                      MERCHANT_PROMO_CODE});
#endif

  std::vector<FormFieldData> submitted_autocomplete_fields;
  bool autocomplete_fields_is_autocomplete_enabled = false;
  EXPECT_CALL(*autocomplete_history_manager_, OnWillSubmitFormWithFields(_, _))
      .WillOnce(
          (DoAll(SaveArg<0>(&submitted_autocomplete_fields),
                 SaveArg<1>(&autocomplete_fields_is_autocomplete_enabled))));

  std::vector<FormFieldData> submitted_merchant_promo_code_fields;
  bool merchant_promo_code_fields_is_autocomplete_enabled = false;
  EXPECT_CALL(*merchant_promo_code_manager_, OnWillSubmitFormWithFields(_, _))
      .WillOnce((DoAll(
          SaveArg<0>(&submitted_merchant_promo_code_fields),
          SaveArg<1>(&merchant_promo_code_fields_is_autocomplete_enabled))));

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  std::vector<FormFieldData> submitted_iban_fields;
  bool iban_fields_is_autocomplete_enabled = false;
  EXPECT_CALL(*iban_manager_, OnWillSubmitFormWithFields(_, _))
      .WillOnce((DoAll(SaveArg<0>(&submitted_iban_fields),
                       SaveArg<1>(&iban_fields_is_autocomplete_enabled))));
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

  single_field_form_fill_router_->OnWillSubmitForm(
      form_data, &form_structure, /*is_autocomplete_enabled=*/true);

  EXPECT_TRUE(submitted_autocomplete_fields.size() ==
              number_of_fields_for_testing);
  EXPECT_TRUE(autocomplete_fields_is_autocomplete_enabled);

  EXPECT_TRUE(submitted_merchant_promo_code_fields.size() ==
              number_of_fields_for_testing);
  EXPECT_TRUE(merchant_promo_code_fields_is_autocomplete_enabled);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  EXPECT_TRUE(submitted_iban_fields.size() == number_of_fields_for_testing);
  EXPECT_TRUE(iban_fields_is_autocomplete_enabled);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

// Ensure that the router routes to SingleFieldFormFillers for this
// CancelPendingQueries call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToAllSingleFieldFormFillers_CancelPendingQueries) {
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  EXPECT_CALL(*autocomplete_history_manager_, CancelPendingQueries);

  EXPECT_CALL(*merchant_promo_code_manager_, CancelPendingQueries);

  EXPECT_CALL(*iban_manager_, CancelPendingQueries);

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
      PopupItemId::kAutocompleteEntry);
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnSingleFieldSuggestionSelected call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToAutocompleteHistoryManager_OnSingleFieldSuggestionSelected) {
  EXPECT_CALL(*autocomplete_history_manager_, OnSingleFieldSuggestionSelected);

  single_field_form_fill_router_->OnSingleFieldSuggestionSelected(
      /*value=*/u"Value", PopupItemId::kAutocompleteEntry);
}

// Ensure that the router routes to MerchantPromoCodeManager for this
// OnGetSingleFieldSuggestions call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToMerchantPromoCodeManager_OnGetSingleFieldSuggestions) {
  for (bool test_field_should_autocomplete : {true, false}) {
    SCOPED_TRACE(testing::Message() << "test_field_should_autocomplete = "
                                    << test_field_should_autocomplete);
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kAutofillFillMerchantPromoCodeFields}, {});
    auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();
    test_field_.should_autocomplete = test_field_should_autocomplete;

    // `test_field_.should_autocomplete` should not affect merchant promo code
    // autofill, so MerchantPromoCodeManager::OnGetSingleFieldSuggestions()
    // should always be called since the given test field is a merchant promo
    // code field.
    EXPECT_CALL(*merchant_promo_code_manager_, OnGetSingleFieldSuggestions)
        .Times(1)
        .WillOnce(testing::Return(true));

    EXPECT_TRUE(single_field_form_fill_router_->OnGetSingleFieldSuggestions(
        AutofillSuggestionTriggerSource::kFormControlElementClicked,
        test_field_, autofill_client_, suggestions_handler->GetWeakPtr(),
        SuggestionsContext()));
  }
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnGetSingleFieldSuggestions call if MerchantPromoCodeManager is not present.
TEST_F(SingleFieldFormFillRouterTest, MerchantPromoCodeManagerNotPresent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kAutofillFillMerchantPromoCodeFields}, {});
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  // This also invalidates the WeakPtr that the `single_field_form_fill_router_`
  // holds on the promo code manager.
  merchant_promo_code_manager_.reset();

  // As the merchant promo code manager is gone, we should call
  // AutocompleteHistoryManager::OnGetSingleFieldSuggestions().
  EXPECT_CALL(*autocomplete_history_manager_, OnGetSingleFieldSuggestions)
      .Times(1)
      .WillOnce(testing::Return(true));

  // As `test_field_.should_autocomplete` is true, this was a valid field for
  // autocomplete. SingleFieldFormFillRouter::OnGetSingleFieldSuggestions()
  // should return true.
  EXPECT_TRUE(single_field_form_fill_router_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnGetSingleFieldSuggestions call if
// MerchantPromoCodeManager::OnGetSingleFieldSuggestions() returns false.
TEST_F(SingleFieldFormFillRouterTest, MerchantPromoCodeManagerReturnedFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kAutofillFillMerchantPromoCodeFields}, {});
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  // Mock MerchantPromoCodeManager::OnGetSingleFieldSuggestions() returning
  // false.
  EXPECT_CALL(*merchant_promo_code_manager_, OnGetSingleFieldSuggestions)
      .Times(1)
      .WillOnce(testing::Return(false));

  // Since MerchantPromoCodeManager::OnGetSingleFieldSuggestions() returned
  // false, we should call
  // AutocompleteHistoryManager::OnGetSingleFieldSuggestions().
  EXPECT_CALL(*autocomplete_history_manager_, OnGetSingleFieldSuggestions)
      .Times(1)
      .WillOnce(testing::Return(true));

  // As `test_field_.should_autocomplete` is true, this was a valid field for
  // autocomplete. SingleFieldFormFillRouter::OnGetSingleFieldSuggestions()
  // should return true.
  EXPECT_TRUE(single_field_form_fill_router_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));
}

// Ensure that the router routes to MerchantPromoCodeManager for this
// OnRemoveCurrentSingleFieldSuggestion call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToMerchantPromoCodeManager_OnRemoveCurrentSingleFieldSuggestion) {
  EXPECT_CALL(*merchant_promo_code_manager_,
              OnRemoveCurrentSingleFieldSuggestion);

  single_field_form_fill_router_->OnRemoveCurrentSingleFieldSuggestion(
      /*field_name=*/u"Field Name", /*value=*/u"Value",
      PopupItemId::kMerchantPromoCodeEntry);
}

// Ensure that the router routes to MerchantPromoCodeManager for this
// OnSingleFieldSuggestionSelected call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToMerchantPromoCodeManager_OnSingleFieldSuggestionSelected) {
  EXPECT_CALL(*merchant_promo_code_manager_, OnSingleFieldSuggestionSelected);

  single_field_form_fill_router_->OnSingleFieldSuggestionSelected(
      /*value=*/u"Value", PopupItemId::kMerchantPromoCodeEntry);
}

// Ensure that SingleFieldFormFillRouter::OnGetSingleFieldSuggestions() returns
// false if all single field form fillers returned false.
TEST_F(
    SingleFieldFormFillRouterTest,
    FieldNotEligibleForAnySingleFieldFormFiller_OnGetSingleFieldSuggestions) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      {features::kAutofillFillMerchantPromoCodeFields}, {});
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  EXPECT_CALL(*merchant_promo_code_manager_, OnGetSingleFieldSuggestions)
      .Times(1)
      .WillOnce(testing::Return(false));

  EXPECT_CALL(*autocomplete_history_manager_, OnGetSingleFieldSuggestions)
      .Times(1)
      .WillOnce(testing::Return(false));

  // All SingleFieldFormFillers returned false, so we should return false as we
  // did not attempt to display any single field form fill suggestions.
  EXPECT_FALSE(single_field_form_fill_router_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnGetSingleFieldSuggestions call if IBANManager is not present.
TEST_F(SingleFieldFormFillRouterTest, IBANManagerNotPresent) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kAutofillParseIBANFields}, {});
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  // This also invalidates the WeakPtr that the |single_field_form_fill_router_|
  // holds on the iban manager.
  iban_manager_.reset();

  // As the IBANmanager is gone, we should call
  // AutocompleteHistoryManager::OnGetSingleFieldSuggestions().
  EXPECT_CALL(*autocomplete_history_manager_, OnGetSingleFieldSuggestions)
      .Times(1)
      .WillOnce(testing::Return(true));

  // As `test_field_.should_autocomplete` is true, this was a valid field for
  // autocomplete. SingleFieldFormFillRouter::OnGetSingleFieldSuggestions()
  // should return true.
  EXPECT_TRUE(single_field_form_fill_router_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnGetSingleFieldSuggestions call if
// IBANManager::OnGetSingleFieldSuggestions() returns false.
TEST_F(SingleFieldFormFillRouterTest, IBANManagerReturnedFalse) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures({features::kAutofillParseIBANFields}, {});
  auto suggestions_handler = std::make_unique<MockSuggestionsHandler>();

  // Mock IBANManager::OnGetSingleFieldSuggestions() returning
  // false.
  EXPECT_CALL(*iban_manager_, OnGetSingleFieldSuggestions)
      .Times(1)
      .WillOnce(testing::Return(false));

  // Since IBANManager::OnGetSingleFieldSuggestions() returned
  // false, we should call
  // AutocompleteHistoryManager::OnGetSingleFieldSuggestions().
  EXPECT_CALL(*autocomplete_history_manager_, OnGetSingleFieldSuggestions)
      .Times(1)
      .WillOnce(testing::Return(true));

  // As `test_field_.should_autocomplete` is true, this was a valid field for
  // autocomplete. SingleFieldFormFillRouter::OnGetSingleFieldSuggestions()
  // should return true.
  EXPECT_TRUE(single_field_form_fill_router_->OnGetSingleFieldSuggestions(
      AutofillSuggestionTriggerSource::kFormControlElementClicked, test_field_,
      autofill_client_, suggestions_handler->GetWeakPtr(),
      SuggestionsContext()));
}

// Ensure that the router routes to IBANManager for this
// OnRemoveCurrentSingleFieldSuggestion call.
TEST_F(SingleFieldFormFillRouterTest,
       RouteToIBANManager_OnRemoveCurrentSingleFieldSuggestion) {
  EXPECT_CALL(*iban_manager_, OnRemoveCurrentSingleFieldSuggestion);

  single_field_form_fill_router_->OnRemoveCurrentSingleFieldSuggestion(
      /*field_name=*/u"Field Name", /*value=*/u"Value",
      PopupItemId::kIbanEntry);
}

}  // namespace autofill
