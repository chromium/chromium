// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_fill_router.h"

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/mock_merchant_promo_code_manager.h"
#include "components/autofill/core/browser/payments/test/mock_iban_manager.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/version_info/version_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::_;
using ::testing::DoAll;
using ::testing::SaveArg;
using ::testing::SizeIs;

class SingleFieldFillRouterTest : public testing::Test {
 protected:
  SingleFieldFillRouterTest()
      : iban_manager_(&personal_data_manager_),
        single_field_fill_router_(&autocomplete_history_manager_,
                                  &iban_manager_,
                                  &merchant_promo_code_manager_) {
    prefs_ = test::PrefServiceForTesting();

    // Mock such that we don't trigger the cleanup.
    prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                       CHROME_VERSION_MAJOR);
    web_data_service_ = base::MakeRefCounted<MockAutofillWebDataService>();
    autocomplete_history_manager_.Init(web_data_service_, prefs_.get(), false);
    merchant_promo_code_manager_.Init(&personal_data_manager_,
                                      /*is_off_the_record=*/false);

    FormData form;
    test_api(form).Append(
        test::CreateTestFormField(/*label=*/"", "Some Field Name", "SomePrefix",
                                  FormControlType::kInputText));
    form_structure_ = std::make_unique<FormStructure>(form);
  }

  FormStructure& form() { return *form_structure_; }
  AutofillField& field() { return *form_structure_->fields().front(); }

  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  TestPersonalDataManager personal_data_manager_;
  scoped_refptr<MockAutofillWebDataService> web_data_service_;
  std::unique_ptr<PrefService> prefs_;
  MockAutocompleteHistoryManager autocomplete_history_manager_;
  MockIbanManager iban_manager_;
  MockMerchantPromoCodeManager merchant_promo_code_manager_;
  SingleFieldFillRouter single_field_fill_router_;

 private:
  std::unique_ptr<FormStructure> form_structure_;
};

// Tests that FormStructure and AutofillField nullptrs are handled by
// SingleFieldFillRouter.
TEST_F(SingleFieldFillRouterTest, TolerateNullPtrs) {
  EXPECT_FALSE(single_field_fill_router_.OnGetSingleFieldSuggestions(
      nullptr, field(), &field(), autofill_client_, base::DoNothing()));
  EXPECT_FALSE(single_field_fill_router_.OnGetSingleFieldSuggestions(
      &form(), field(), nullptr, autofill_client_, base::DoNothing()));
  EXPECT_FALSE(single_field_fill_router_.OnGetSingleFieldSuggestions(
      &form(), field(), &field(), autofill_client_, {}));
  single_field_fill_router_.OnWillSubmitForm(form().ToFormData(), nullptr,
                                             /*is_autocomplete_enabled=*/true);
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnGetSingleFieldSuggestions call if the field has autocomplete on.
TEST_F(SingleFieldFillRouterTest,
       RouteToAutocompleteHistoryManager_OnGetSingleFieldSuggestions) {
  for (bool test_field_should_autocomplete : {true, false}) {
    SCOPED_TRACE(testing::Message() << "test_field_should_autocomplete = "
                                    << test_field_should_autocomplete);
    field().set_should_autocomplete(test_field_should_autocomplete);

    // If `field().should_autocomplete` is true, that means autocomplete is
    // turned on for the given test field and
    // AutocompleteHistoryManager::OnGetSingleFieldSuggestions() should return
    // true. If `field().should_autocomplete` is false, then autocomplete is
    // turned off for the given test field and
    // AutocompleteHistoryManager::OnGetSingleFieldSuggestions() should return
    // false.
    EXPECT_CALL(autocomplete_history_manager_, OnGetSingleFieldSuggestions)
        .WillOnce(testing::Return(field().should_autocomplete()));

    EXPECT_EQ(
        field().should_autocomplete(),
        single_field_fill_router_.OnGetSingleFieldSuggestions(
            &form(), field(), &field(), autofill_client_, base::DoNothing()));
  }
}

// Ensure that the router routes to all fillers for this OnWillSubmitForm call,
// and call OnWillSubmitFormWithFields if corresponding manager (e.g.,
// IbanManager) presents.
TEST_F(SingleFieldFillRouterTest, RouteToAllFillers_OnWillSubmitForm) {
  FormData form_data;
  size_t number_of_fields_for_testing = 3;
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  test_api(form_data).Resize(3 * number_of_fields_for_testing);
#else
  test_api(form_data).Resize(2 * number_of_fields_for_testing);
#endif

  FormStructure form_structure{form_data};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  test_api(form_structure)
      .SetFieldTypes({UNKNOWN_TYPE, UNKNOWN_TYPE, UNKNOWN_TYPE,
                      MERCHANT_PROMO_CODE, MERCHANT_PROMO_CODE,
                      MERCHANT_PROMO_CODE, IBAN_VALUE, IBAN_VALUE, IBAN_VALUE});
#else
  test_api(form_structure)
      .SetFieldTypes({UNKNOWN_TYPE, UNKNOWN_TYPE, UNKNOWN_TYPE,
                      MERCHANT_PROMO_CODE, MERCHANT_PROMO_CODE,
                      MERCHANT_PROMO_CODE});
#endif

  EXPECT_CALL(
      autocomplete_history_manager_,
      OnWillSubmitFormWithFields(SizeIs(number_of_fields_for_testing), true));
  single_field_fill_router_.OnWillSubmitForm(form_data, &form_structure,
                                             /*is_autocomplete_enabled=*/true);
}

// Ensure that the router routes to fillers for this CancelPendingQueries call.
TEST_F(SingleFieldFillRouterTest, RouteToAllFillers_CancelPendingQueries) {
  EXPECT_CALL(autocomplete_history_manager_, CancelPendingQueries);
  single_field_fill_router_.CancelPendingQueries();
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnRemoveCurrentSingleFieldSuggestion call.
TEST_F(SingleFieldFillRouterTest,
       RouteToAutocompleteHistoryManager_OnRemoveCurrentSingleFieldSuggestion) {
  EXPECT_CALL(autocomplete_history_manager_,
              OnRemoveCurrentSingleFieldSuggestion);

  single_field_fill_router_.OnRemoveCurrentSingleFieldSuggestion(
      /*field_name=*/u"Field Name", /*value=*/u"Value",
      SuggestionType::kAutocompleteEntry);
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnSingleFieldSuggestionSelected call.
TEST_F(SingleFieldFillRouterTest,
       RouteToAutocompleteHistoryManager_OnSingleFieldSuggestionSelected) {
  EXPECT_CALL(autocomplete_history_manager_, OnSingleFieldSuggestionSelected);

  Suggestion suggestion(u"Value", SuggestionType::kAutocompleteEntry);
  single_field_fill_router_.OnSingleFieldSuggestionSelected(suggestion);
}

// Ensure that the router routes to MerchantPromoCodeManager for this
// OnGetSingleFieldSuggestions call.
TEST_F(SingleFieldFillRouterTest,
       RouteToMerchantPromoCodeManager_OnGetSingleFieldSuggestions) {
  for (bool test_field_should_autocomplete : {true, false}) {
    SCOPED_TRACE(testing::Message() << "test_field_should_autocomplete = "
                                    << test_field_should_autocomplete);
    field().set_should_autocomplete(test_field_should_autocomplete);

    // `field().should_autocomplete` should not affect merchant promo code
    // autofill, so MerchantPromoCodeManager::OnGetSingleFieldSuggestions()
    // should always be called since the given test field is a merchant promo
    // code field.
    EXPECT_CALL(merchant_promo_code_manager_, OnGetSingleFieldSuggestions)
        .WillOnce(testing::Return(true));

    EXPECT_TRUE(single_field_fill_router_.OnGetSingleFieldSuggestions(
        &form(), field(), &field(), autofill_client_, base::DoNothing()));
  }
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnGetSingleFieldSuggestions call if MerchantPromoCodeManager is not present.
TEST_F(SingleFieldFillRouterTest, MerchantPromoCodeManagerNotPresent) {
  SingleFieldFillRouter router(&autocomplete_history_manager_, &iban_manager_,
                               /*merchant_promo_code_manager=*/nullptr);

  // As the merchant promo code manager is gone, we should call
  // AutocompleteHistoryManager::OnGetSingleFieldSuggestions().
  EXPECT_CALL(autocomplete_history_manager_, OnGetSingleFieldSuggestions)
      .WillOnce(testing::Return(true));

  // As `field().should_autocomplete` is true, this was a valid field for
  // autocomplete. SingleFieldFillRouter::OnGetSingleFieldSuggestions()
  // should return true.
  EXPECT_TRUE(router.OnGetSingleFieldSuggestions(
      &form(), field(), &field(), autofill_client_, base::DoNothing()));
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnGetSingleFieldSuggestions call if
// MerchantPromoCodeManager::OnGetSingleFieldSuggestions() returns false.
TEST_F(SingleFieldFillRouterTest, MerchantPromoCodeManagerReturnedFalse) {
  // Mock MerchantPromoCodeManager::OnGetSingleFieldSuggestions() returning
  // false.
  EXPECT_CALL(merchant_promo_code_manager_, OnGetSingleFieldSuggestions)
      .WillOnce(testing::Return(false));

  // Since MerchantPromoCodeManager::OnGetSingleFieldSuggestions() returned
  // false, we should call
  // AutocompleteHistoryManager::OnGetSingleFieldSuggestions().
  EXPECT_CALL(autocomplete_history_manager_, OnGetSingleFieldSuggestions)
      .WillOnce(testing::Return(true));

  // As `field().should_autocomplete` is true, this was a valid field for
  // autocomplete. SingleFieldFillRouter::OnGetSingleFieldSuggestions()
  // should return true.
  EXPECT_TRUE(single_field_fill_router_.OnGetSingleFieldSuggestions(
      &form(), field(), &field(), autofill_client_, base::DoNothing()));
}

// Ensure that the router routes to MerchantPromoCodeManager for this
// OnSingleFieldSuggestionSelected call.
TEST_F(SingleFieldFillRouterTest,
       RouteToMerchantPromoCodeManager_OnSingleFieldSuggestionSelected) {
  EXPECT_CALL(merchant_promo_code_manager_, OnSingleFieldSuggestionSelected);

  Suggestion suggestion(u"Value", SuggestionType::kMerchantPromoCodeEntry);
  single_field_fill_router_.OnSingleFieldSuggestionSelected(suggestion);
}

// Ensure that SingleFieldFillRouter::OnGetSingleFieldSuggestions() returns
// false if all single field form fillers returned false.
TEST_F(SingleFieldFillRouterTest,
       FieldNotEligibleForAnyFillersOnGetSingleFieldSuggestions) {
  EXPECT_CALL(merchant_promo_code_manager_, OnGetSingleFieldSuggestions)
      .WillOnce(testing::Return(false));

  EXPECT_CALL(autocomplete_history_manager_, OnGetSingleFieldSuggestions)
      .WillOnce(testing::Return(false));

  // All fillers returned false, so we should return false as we did not attempt
  // to display any single field form fill suggestions.
  EXPECT_FALSE(single_field_fill_router_.OnGetSingleFieldSuggestions(
      &form(), field(), &field(), autofill_client_, base::DoNothing()));
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnGetSingleFieldSuggestions call if IbanManager is not present.
TEST_F(SingleFieldFillRouterTest, IbanManagerNotPresent) {
  SingleFieldFillRouter router(&autocomplete_history_manager_,
                               /*iban_manager=*/nullptr,
                               &merchant_promo_code_manager_);

  // As the IbanManager is gone, we should call
  // AutocompleteHistoryManager::OnGetSingleFieldSuggestions().
  EXPECT_CALL(autocomplete_history_manager_, OnGetSingleFieldSuggestions)
      .WillOnce(testing::Return(true));

  // As `field().should_autocomplete` is true, this was a valid field for
  // autocomplete. SingleFieldFillRouter::OnGetSingleFieldSuggestions()
  // should return true.
  EXPECT_TRUE(router.OnGetSingleFieldSuggestions(
      &form(), field(), &field(), autofill_client_, base::DoNothing()));
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnGetSingleFieldSuggestions call if
// IbanManager::OnGetSingleFieldSuggestions() returns false.
TEST_F(SingleFieldFillRouterTest, IbanManagerReturnedFalse) {
  // Mock IbanManager::OnGetSingleFieldSuggestions() returning
  // false.
  EXPECT_CALL(iban_manager_, OnGetSingleFieldSuggestions)
      .WillOnce(testing::Return(false));

  // Since IbanManager::OnGetSingleFieldSuggestions() returned
  // false, we should call
  // AutocompleteHistoryManager::OnGetSingleFieldSuggestions().
  EXPECT_CALL(autocomplete_history_manager_, OnGetSingleFieldSuggestions)
      .WillOnce(testing::Return(true));

  // As `field().should_autocomplete` is true, this was a valid field for
  // autocomplete. SingleFieldFillRouter::OnGetSingleFieldSuggestions()
  // should return true.
  EXPECT_TRUE(single_field_fill_router_.OnGetSingleFieldSuggestions(
      &form(), field(), &field(), autofill_client_, base::DoNothing()));
}

}  // namespace
}  // namespace autofill
