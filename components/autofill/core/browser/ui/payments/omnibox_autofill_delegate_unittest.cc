// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/omnibox_autofill_delegate.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/mock_autofill_manager_observer.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/payments/omnibox_autofill_metrics.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using autofill_metrics::OmniboxAutofillShowChipDecisionPart1;
using test::CreateFormDataForFrame;
using test::CreateTestFormField;

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(AutofillManager*,
              GetAutofillManagerForPrimaryMainFrame,
              (),
              (override));
};

class OmniboxAutofillDelegateTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<MockAutofillClient> {
 public:
  OmniboxAutofillDelegateTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableOmniboxAutofill);
  }

  ~OmniboxAutofillDelegateTest() override = default;

  void SetUp() override {
    InitAutofillClient();

    // Set up the PaymentsDataManager and add a masked server card by default.
    autofill_client().GetPersonalDataManager().set_payments_data_manager(
        std::make_unique<TestPaymentsDataManager>());
    autofill_client()
        .GetPersonalDataManager()
        .test_payments_data_manager()
        .SetPrefService(autofill_client().GetPrefs());
    autofill_client()
        .GetPersonalDataManager()
        .payments_data_manager()
        .SetSyncingForTest(true);
    autofill_client()
        .GetPersonalDataManager()
        .test_payments_data_manager()
        .AddCreditCard(test::GetMaskedServerCard());

    CreateAutofillDriver();
    autofill_driver().SetParent(nullptr);
    autofill_driver().SetIsEmbedded(false);
    autofill_driver().SetIsActive(true);

    ON_CALL(autofill_client(), GetAutofillManagerForPrimaryMainFrame)
        .WillByDefault(::testing::Return(&autofill_manager()));
  }

  void TearDown() override { DestroyAutofillClient(); }

  void FormsSeen(const std::vector<FormData>& forms) {
    autofill_manager().OnFormsSeen(/*updated_forms=*/forms,
                                   /*removed_forms=*/{});
  }

  FormData CreateTestCreditCardFormData() {
    FormData form;
    AppendTestCreditCardFormData(&form);
    return form;
  }

 private:
  // Populates `form` with data corresponding to a simple credit card form.
  void AppendTestCreditCardFormData(FormData* form) {
    form->set_name(u"MyForm");
    form->set_url(GURL("https://myform.com/form.html"));
    form->set_action(GURL("https://myform.com/submit.html"));
    autofill_client().set_last_committed_primary_main_frame_url(form->url());

    test_api(*form).Append(CreateTestFormField("Name on Card", "nameoncard", "",
                                               FormControlType::kInputText));
    test_api(*form).Append(CreateTestFormField("Card Number", "cardnumber", "",
                                               FormControlType::kInputText));
    test_api(*form).Append(CreateTestFormField("Expiration Date", "ccmonth", "",
                                               FormControlType::kInputText));
    test_api(*form).Append(
        CreateTestFormField("", "ccyear", "", FormControlType::kInputText));
    test_api(*form).Append(
        CreateTestFormField("CVC", "cvc", "", FormControlType::kInputText));

    *form = CreateFormDataForFrame(*form, autofill_driver().GetFrameToken());
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(OmniboxAutofillDelegateTest, OnFieldTypesDetermined_SuccessPath) {
  base::HistogramTester histogram_tester;

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kSuccess, 1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_CalledFromNonOutermostDriver_Aborts) {
  base::HistogramTester histogram_tester;

  // If the main AutofillDriver has a parent, it's not the right
  // BrowserAutofillManager to run OmniboxAutofillDelegate logic.
  CreateAutofillDriver();
  autofill_driver(0).SetParent(&autofill_driver(1));

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kNotActiveOutermostMainFrameBam, 1);

  autofill_driver(0).SetParent(nullptr);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_CalledFromEmbeddedDriver_Aborts) {
  base::HistogramTester histogram_tester;

  // If the main AutofillDriver is embedded, it's not the right
  // BrowserAutofillManager to run OmniboxAutofillDelegate logic.
  autofill_driver().SetIsEmbedded(true);

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kNotActiveOutermostMainFrameBam, 1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_CalledFromInactiveDriver_Aborts) {
  base::HistogramTester histogram_tester;

  // If the main AutofillDriver is inactive, it's not the right
  // BrowserAutofillManager to run OmniboxAutofillDelegate logic.
  autofill_driver().SetIsActive(false);

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kNotActiveOutermostMainFrameBam, 1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_AutofillPolicyDisabled_Aborts) {
  base::HistogramTester histogram_tester;

  // Do not run Omnibox functionality if payment method Autofill is disabled.
  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::
          kAutofillPaymentMethodsPolicyDisabled,
      1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_NoCreditCardsSaved_Aborts) {
  base::HistogramTester histogram_tester;

  // Specifically remove all credit cards from PaymentsDataManager.
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kNoCreditCardsSaved, 1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_CreditCardSaved_CanBeLocalCard) {
  base::HistogramTester histogram_tester;

  // Specifically add a local credit card to PaymentsDataManager.
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .AddCreditCard(test::GetCreditCard());

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kSuccess, 1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_CreditCardSaved_CanBeServerCard) {
  base::HistogramTester histogram_tester;

  // Specifically add a masked server credit card to PaymentsDataManager.
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .ClearCreditCards();
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .AddCreditCard(test::GetMaskedServerCard());

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kSuccess, 1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_MissingCreditCardNumberField_Aborts) {
  base::HistogramTester histogram_tester;

  // Create a credit card form, but don't include a card number field.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  autofill_client().set_last_committed_primary_main_frame_url(form.url());
  test_api(form).Append(CreateTestFormField("Name on Card", "nameoncard", "",
                                            FormControlType::kInputText));
  test_api(form).Append(CreateTestFormField("Expiration Date", "ccmonth", "",
                                            FormControlType::kInputText));
  test_api(form).Append(
      CreateTestFormField("", "ccyear", "", FormControlType::kInputText));
  test_api(form).Append(
      CreateTestFormField("CVC", "cvc", "", FormControlType::kInputText));
  form = CreateFormDataForFrame(form, autofill_driver().GetFrameToken());

  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kNotCompleteCreditCardForm, 1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_MissingCreditCardExpiration_Aborts) {
  base::HistogramTester histogram_tester;

  // Create a credit card form, but don't include a expiration date fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  autofill_client().set_last_committed_primary_main_frame_url(form.url());
  test_api(form).Append(CreateTestFormField("Name on Card", "nameoncard", "",
                                            FormControlType::kInputText));
  test_api(form).Append(CreateTestFormField("Card Number", "cardnumber", "",
                                            FormControlType::kInputText));
  test_api(form).Append(
      CreateTestFormField("CVC", "cvc", "", FormControlType::kInputText));
  form = CreateFormDataForFrame(form, autofill_driver().GetFrameToken());

  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kNotCompleteCreditCardForm, 1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_AcceptsMinimalCreditCardForm) {
  base::HistogramTester histogram_tester;

  // Create a credit card form, including only card number and expiration.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  autofill_client().set_last_committed_primary_main_frame_url(form.url());
  test_api(form).Append(CreateTestFormField("Card Number", "cardnumber", "",
                                            FormControlType::kInputText));
  test_api(form).Append(CreateTestFormField("Expiration Date", "ccmonth", "",
                                            FormControlType::kInputText));
  test_api(form).Append(
      CreateTestFormField("", "ccyear", "", FormControlType::kInputText));
  form = CreateFormDataForFrame(form, autofill_driver().GetFrameToken());

  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kSuccess, 1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_FormNotSecure_Aborts) {
  base::HistogramTester histogram_tester;

  // Create a credit card form, specifically using http:// instead of https://.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("http://myform.com/form.html"));
  form.set_action(GURL("http://myform.com/submit.html"));
  autofill_client().set_last_committed_primary_main_frame_url(form.url());
  test_api(form).Append(CreateTestFormField("Card Number", "cardnumber", "",
                                            FormControlType::kInputText));
  test_api(form).Append(CreateTestFormField("Expiration Date", "ccmonth", "",
                                            FormControlType::kInputText));
  test_api(form).Append(
      CreateTestFormField("", "ccyear", "", FormControlType::kInputText));
  form = CreateFormDataForFrame(form, autofill_driver().GetFrameToken());

  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kFormOrClientContextNotSecure, 1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_MultipleCreditCardNumberFields_Aborts) {
  base::HistogramTester histogram_tester;

  // Create a credit card form, but include multiple card number fields.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  autofill_client().set_last_committed_primary_main_frame_url(form.url());
  test_api(form).Append(CreateTestFormField("Name on Card", "nameoncard", "",
                                            FormControlType::kInputText));
  test_api(form).Append(CreateTestFormField("Card Number 1", "cardnumber1", "",
                                            FormControlType::kInputText));
  test_api(form).Append(CreateTestFormField("Card Number 2", "cardnumber2", "",
                                            FormControlType::kInputText));
  test_api(form).Append(CreateTestFormField("Expiration Date", "ccmonth", "",
                                            FormControlType::kInputText));
  test_api(form).Append(
      CreateTestFormField("", "ccyear", "", FormControlType::kInputText));
  test_api(form).Append(
      CreateTestFormField("CVC", "cvc", "", FormControlType::kInputText));
  form = CreateFormDataForFrame(form, autofill_driver().GetFrameToken());

  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::
          kFoundMultipleCreditCardNumberFields,
      1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_OptimizationGuideDeciderMissing_Aborts) {
  base::HistogramTester histogram_tester;

  // Create a credit card form with card number and expiration, but put the
  // fields in iframes so dealing with the OptimizationGuideDeicder is required.
  FormData form;
  form.set_name(u"MyForm");
  form.set_url(GURL("https://myform.com/form.html"));
  form.set_action(GURL("https://myform.com/submit.html"));
  url::Origin field_origin =
      url::Origin::CreateFromNormalizedTuple("https", "someothersite.com", 80);
  autofill_client().set_last_committed_primary_main_frame_url(form.url());
  FormFieldData card_number_field = CreateTestFormField(
      "Card Number", "cardnumber", "", FormControlType::kInputText);
  card_number_field.set_origin(field_origin);
  test_api(form).Append(card_number_field);
  FormFieldData exp_month_field = CreateTestFormField(
      "Expiration Date", "ccmonth", "", FormControlType::kInputText);
  exp_month_field.set_origin(field_origin);
  test_api(form).Append(exp_month_field);
  FormFieldData exp_year_field =
      CreateTestFormField("", "ccyear", "", FormControlType::kInputText);
  exp_year_field.set_origin(field_origin);
  test_api(form).Append(exp_year_field);
  // Then, get rid of the OptimizationGuideDecider, as if it returned `nullptr`.
  autofill_client().ResetAutofillOptimizationGuideDecider();

  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kMissingOptimizationGuideDecider,
      1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_CandidateFormFound_ReturnsEarly) {
  FormData form = CreateTestCreditCardFormData();

  // `FormsSeen(~)` will successfully run all OmniboxAutofillDelegate checks and
  // mark `form` as the candidate form to trigger Omnibox Autofill.
  {
    base::HistogramTester histogram_tester;
    FormsSeen({form});
    histogram_tester.ExpectUniqueSample(
        "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
        OmniboxAutofillShowChipDecisionPart1::kSuccess, 1);
  }

  // Because a candidate form was already found, all future check logic will be
  // skipped, proven by the lack of new UMA logs coming from
  // `OnFieldTypesDetermined(~)`.
  {
    base::HistogramTester histogram_tester;
    FormsSeen({form});
    histogram_tester.ExpectTotalCount(
        "Autofill.OmniboxAutofill.ShowChipDecisionPart1", 0);
  }
}

TEST_F(OmniboxAutofillDelegateTest,
       OnAutofillManagerStateChanged_WasActive_HideChip) {
  payments_autofill_client().ShowOmniboxAutofillChip(
      /*suggestions=*/{},
      /*on_suggestions_shown=*/base::DoNothing(),
      /*on_suggestions_hidden=*/base::DoNothing(),
      /*did_select_suggestion=*/base::DoNothing(),
      /*did_accept_suggestion=*/base::DoNothing());

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnAutofillManagerStateChanged(
      autofill_manager(), /*previous=*/AutofillManager::LifecycleState::kActive,
      /*current=*/AutofillManager::LifecycleState::kInactive);

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_hidden());
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnAutofillManagerStateChanged_WasActive_ResetsState) {
  FormData form = CreateTestCreditCardFormData();

  // Find a candidate form. This sets `candidate_form_found_` to `true`.
  {
    base::HistogramTester histogram_tester;
    FormsSeen({form});
    histogram_tester.ExpectUniqueSample(
        "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
        OmniboxAutofillShowChipDecisionPart1::kSuccess, 1);
  }

  // Trigger state change to inactive (from active), which triggers `Reset()`.
  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);
  delegate->OnAutofillManagerStateChanged(
      autofill_manager(), /*previous=*/AutofillManager::LifecycleState::kActive,
      /*current=*/AutofillManager::LifecycleState::kInactive);

  // Verify that the state was reset. If it was reset, we can find the candidate
  // form again and it will log success.
  {
    base::HistogramTester histogram_tester;
    FormsSeen({form});
    histogram_tester.ExpectUniqueSample(
        "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
        OmniboxAutofillShowChipDecisionPart1::kSuccess, 1);
  }
}

TEST_F(OmniboxAutofillDelegateTest,
       OnAutofillManagerStateChanged_WasNotActive_DoesNotHideChip) {
  payments_autofill_client().ShowOmniboxAutofillChip(
      /*suggestions=*/{},
      /*on_suggestions_shown=*/base::DoNothing(),
      /*on_suggestions_hidden=*/base::DoNothing(),
      /*did_select_suggestion=*/base::DoNothing(),
      /*did_accept_suggestion=*/base::DoNothing());

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnAutofillManagerStateChanged(
      autofill_manager(),
      /*previous=*/AutofillManager::LifecycleState::kInactive,
      /*current=*/AutofillManager::LifecycleState::kActive);

  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());
  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest, OnAfterFormsSeen_FormRemoved_HidesChip) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  payments_autofill_client().ShowOmniboxAutofillChip(
      /*suggestions=*/{},
      /*on_suggestions_shown=*/base::DoNothing(),
      /*on_suggestions_hidden=*/base::DoNothing(),
      /*did_select_suggestion=*/base::DoNothing(),
      /*did_accept_suggestion=*/base::DoNothing());

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{form.global_id()});

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_hidden());
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest, OnAfterFormsSeen_FormRemoved_ResetsState) {
  FormData form = CreateTestCreditCardFormData();

  // Find a candidate form. This sets `candidate_form_found_` to `true`.
  {
    base::HistogramTester histogram_tester;
    FormsSeen({form});
    histogram_tester.ExpectUniqueSample(
        "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
        OmniboxAutofillShowChipDecisionPart1::kSuccess, 1);
  }

  // Remove the form. This should trigger `OnAfterFormsSeen` and `Reset()`.
  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{form.global_id()});

  // Verify that the state was reset. If it was reset, we can find the candidate
  // form again.
  {
    base::HistogramTester histogram_tester;
    FormsSeen({form});
    histogram_tester.ExpectUniqueSample(
        "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
        OmniboxAutofillShowChipDecisionPart1::kSuccess, 1);
  }
}

TEST_F(OmniboxAutofillDelegateTest,
       OnAfterFormsSeen_FormNotRemoved_DoesNotHideChip) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  payments_autofill_client().ShowOmniboxAutofillChip(
      /*suggestions=*/{},
      /*on_suggestions_shown=*/base::DoNothing(),
      /*on_suggestions_hidden=*/base::DoNothing(),
      /*did_select_suggestion=*/base::DoNothing(),
      /*did_accept_suggestion=*/base::DoNothing());

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());

  FormGlobalId different_form_id = test::MakeFormGlobalId();
  ASSERT_NE(different_form_id, form.global_id());
  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{different_form_id});

  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());
  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnAfterFormsSeen_CandidateFormNotFound_ReturnsEarly) {
  FormGlobalId form_id = test::MakeFormGlobalId();
  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{form_id});

  // Since a candidate form has not been found yet (`candidate_form_found_` is
  // false), checking `form_id` against the uninitialized
  // `trigger_form_global_id_` would be pointless. Instead, abort hide logic.
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnGetIntersectionObserverInfo_NotVisible_ReturnsEarly) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnGetIntersectionObserverInfo(/*is_visible=*/false);

  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnGetIntersectionObserverInfo_NoAutofillManager_ReturnsEarly) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  // Override mock to return `nullptr`.
  EXPECT_CALL(autofill_client(), GetAutofillManagerForPrimaryMainFrame)
      .WillOnce(::testing::Return(nullptr));

  delegate->OnGetIntersectionObserverInfo(/*is_visible=*/true);

  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnGetIntersectionObserverInfo_FormNotFound_ReturnsEarly) {
  // Do not call `FormsSeen` to simulate form not found.

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnGetIntersectionObserverInfo(/*is_visible=*/true);

  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnGetIntersectionObserverInfo_FieldNotFound_ReturnsEarly) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  // Update the form to remove the trigger field (card number) at index 1 (see
  // `AppendTestCreditCardFormData`).
  FormData updated_form = form;
  ASSERT_EQ(test_api(updated_form).fields().size(), 5u);
  test_api(updated_form).Remove(1);
  FormsSeen({updated_form});

  delegate->OnGetIntersectionObserverInfo(/*is_visible=*/true);

  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnGetIntersectionObserverInfo_IsVisible_ShowsChip) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnGetIntersectionObserverInfo(/*is_visible=*/true);

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnGetIntersectionObserverInfo_IsVisible_LogsMetrics) {
  base::HistogramTester histogram_tester;

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnGetIntersectionObserverInfo(/*is_visible=*/true);

  // Verify that suggestions count and secure form are logged. `SetUp()` adds 1
  // credit card, so count should be 1.
  histogram_tester.ExpectUniqueSample("Autofill.SuggestionsCount.CreditCard", 1,
                                      1);
  histogram_tester.ExpectUniqueSample("Autofill.QueriedCreditCardFormIsSecure",
                                      true, 1);
}

TEST_F(OmniboxAutofillDelegateTest, OnSuggestionsShown_ForwardToObserver) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  MockAutofillManagerObserver observer;
  autofill_manager().AddObserver(&observer);

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry)};

  EXPECT_CALL(observer, OnSuggestionsShown(::testing::Ref(autofill_manager()),
                                           testing::_));
  delegate->OnSuggestionsShown(suggestions, std::nullopt);

  autofill_manager().RemoveObserver(&observer);
}

TEST_F(OmniboxAutofillDelegateTest, OnSuggestionsShown_LogFormEvents) {
  base::HistogramTester histogram_tester;

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry)};

  delegate->OnSuggestionsShown(suggestions, std::nullopt);

  // Verify interaction and shown form events.
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      autofill_metrics::FORM_EVENT_INTERACTED_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);

  // Verify form events log with the correct data suffix. `SetUp()` adds a
  // masked server card, so it should log under ".WithOnlyServerData".
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.WithOnlyServerData",
      autofill_metrics::FORM_EVENT_INTERACTED_ONCE, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.WithOnlyServerData",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.WithOnlyServerData",
      autofill_metrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
}

TEST_F(OmniboxAutofillDelegateTest, OnSuggestionsShown_LogTimingMetrics) {
  base::HistogramTester histogram_tester;

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry)};

  delegate->OnSuggestionsShown(suggestions, std::nullopt);

  // Verify timing metrics logging.
  histogram_tester.ExpectTotalCount(
      "Autofill.Timing.ParseFormUntilInteraction2", 1);
}

TEST_F(OmniboxAutofillDelegateTest, OnSuggestionsShown_LogFunnelMetrics) {
  base::HistogramTester histogram_tester;

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry)};

  delegate->OnSuggestionsShown(suggestions, std::nullopt);

  // Reset the manager to trigger logger destruction and metrics logging.
  autofill_manager().Reset();

  histogram_tester.ExpectUniqueSample("Autofill.Funnel.ParsedAsType.CreditCard",
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Funnel.ParsedAsType.StandaloneCvc", false, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Funnel.InteractionAfterParsedAsType.CreditCard", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Funnel.SuggestionAfterInteraction.CreditCard", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.Funnel.FillAfterSuggestion.CreditCard", false, 1);
}

TEST_F(OmniboxAutofillDelegateTest, OnSuggestionsShown_DoesNotLogKeyMetrics) {
  base::HistogramTester histogram_tester;

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry)};

  delegate->OnSuggestionsShown(suggestions, std::nullopt);

  // Reset the manager to trigger logger destruction and metrics logging.
  autofill_manager().Reset();

  // Key metrics are only logged upon submission. Since there was no submission,
  // they should not be logged.
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingReadiness.CreditCard", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAcceptance.CreditCard", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingCorrectness.CreditCard", 0);
  histogram_tester.ExpectTotalCount(
      "Autofill.KeyMetrics.FillingAssistance.CreditCard", 0);
}

TEST_F(OmniboxAutofillDelegateTest, OnSuggestionsHidden_ForwardToObserver) {
  MockAutofillManagerObserver observer;
  autofill_manager().AddObserver(&observer);

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  EXPECT_CALL(observer,
              OnSuggestionsHidden(::testing::Ref(autofill_manager()),
                                  SuggestionHidingReason::kUserAborted));
  delegate->OnSuggestionsHidden(SuggestionHidingReason::kUserAborted);

  autofill_manager().RemoveObserver(&observer);
}

}  // namespace

}  // namespace autofill
