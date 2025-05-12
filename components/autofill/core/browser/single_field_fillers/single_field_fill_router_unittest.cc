// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/single_field_fillers/single_field_fill_router.h"

#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/version_info/version_info.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/payments/test/mock_iban_manager.h"
#include "components/autofill/core/browser/single_field_fillers/autocomplete/mock_autocomplete_history_manager.h"
#include "components/autofill/core/browser/single_field_fillers/payments/mock_merchant_promo_code_manager.h"
#include "components/autofill/core/browser/suggestions/suggestions_context.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
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
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SizeIs;

class SingleFieldFillRouterTest : public testing::Test {
 protected:
  SingleFieldFillRouterTest()
      : iban_manager_(&personal_data_manager().payments_data_manager()),
        single_field_fill_router_(&history_manager(),
                                  &iban_manager(),
                                  &promo_code_manager()) {
    prefs_ = test::PrefServiceForTesting();

    // Mock such that we don't trigger the cleanup.
    prefs_->SetInteger(prefs::kAutocompleteLastVersionRetentionPolicy,
                       version_info::GetMajorVersionNumberAsInt());
    web_data_service_ = base::MakeRefCounted<MockAutofillWebDataService>();
    history_manager().Init(web_data_service_, prefs_.get(), false);

    FormData form;
    test_api(form).Append(
        test::CreateTestFormField(/*label=*/"", "Some Field Name", "SomePrefix",
                                  FormControlType::kInputText));
    form_structure_ = std::make_unique<FormStructure>(form);
  }

  AutofillClient& client() { return autofill_client_; }
  FormStructure& form() { return *form_structure_; }
  AutofillField& field() { return *form_structure_->fields().front(); }
  MockAutocompleteHistoryManager& history_manager() {
    return autocomplete_history_manager_;
  }
  MockIbanManager& iban_manager() { return iban_manager_; }
  PersonalDataManager& personal_data_manager() {
    return client().GetPersonalDataManager();
  }
  MockMerchantPromoCodeManager& promo_code_manager() {
    return merchant_promo_code_manager_;
  }
  SingleFieldFillRouter& router() { return single_field_fill_router_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  TestAutofillClient autofill_client_;
  scoped_refptr<MockAutofillWebDataService> web_data_service_;
  std::unique_ptr<PrefService> prefs_;
  MockAutocompleteHistoryManager autocomplete_history_manager_;
  MockIbanManager iban_manager_;
  MockMerchantPromoCodeManager merchant_promo_code_manager_{
      &personal_data_manager().payments_data_manager()};
  SingleFieldFillRouter single_field_fill_router_;
  std::unique_ptr<FormStructure> form_structure_;
};

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
      history_manager(),
      OnWillSubmitFormWithFields(SizeIs(number_of_fields_for_testing), true));
  router().OnWillSubmitForm(form_data, &form_structure,
                            /*is_autocomplete_enabled=*/true);
}

// Ensure that the router routes to fillers for this CancelPendingQueries call.
TEST_F(SingleFieldFillRouterTest, RouteToAllFillers_CancelPendingQueries) {
  EXPECT_CALL(history_manager(), CancelPendingQueries);
  router().CancelPendingQueries();
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnRemoveCurrentSingleFieldSuggestion call.
TEST_F(SingleFieldFillRouterTest,
       RouteToAutocompleteHistoryManager_OnRemoveCurrentSingleFieldSuggestion) {
  EXPECT_CALL(history_manager(), OnRemoveCurrentSingleFieldSuggestion);

  router().OnRemoveCurrentSingleFieldSuggestion(
      /*field_name=*/u"Field Name", /*value=*/u"Value",
      SuggestionType::kAutocompleteEntry);
}

// Ensure that the router routes to AutocompleteHistoryManager for this
// OnSingleFieldSuggestionSelected call.
TEST_F(SingleFieldFillRouterTest,
       RouteToAutocompleteHistoryManager_OnSingleFieldSuggestionSelected) {
  EXPECT_CALL(history_manager(), OnSingleFieldSuggestionSelected);

  Suggestion suggestion(u"Value", SuggestionType::kAutocompleteEntry);
  router().OnSingleFieldSuggestionSelected(suggestion);
}

// Ensure that the router routes to MerchantPromoCodeManager for this
// OnSingleFieldSuggestionSelected call.
TEST_F(SingleFieldFillRouterTest,
       RouteToMerchantPromoCodeManager_OnSingleFieldSuggestionSelected) {
  EXPECT_CALL(promo_code_manager(), OnSingleFieldSuggestionSelected);

  Suggestion suggestion(u"Value", SuggestionType::kMerchantPromoCodeEntry);
  router().OnSingleFieldSuggestionSelected(suggestion);
}

}  // namespace
}  // namespace autofill
