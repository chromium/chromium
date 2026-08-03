// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/payments/omnibox_autofill_delegate.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_driver_router.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/mock_autofill_manager_observer.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver_factory.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/metrics/form_events/form_events.h"
#include "components/autofill/core/browser/metrics/payments/omnibox_autofill_metrics.h"
#include "components/autofill/core/browser/payments/test/mock_multiple_request_payments_network_interface.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/ui/autofill_suggestion_delegate.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using autofill_metrics::OmniboxAutofillEvents;
using autofill_metrics::OmniboxAutofillShowChipDecisionPart1;
using autofill_metrics::OmniboxAutofillShowChipDecisionPart2;
using test::CreateFormDataForFrame;
using test::CreateTestFormField;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;

class MockAutofillDriver : public TestAutofillDriver {
 public:
  using TestAutofillDriver::TestAutofillDriver;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;
  ~MockAutofillDriver() override = default;

  MOCK_METHOD(void, RendererShouldClearPreviewedForm, (), (override));
  MOCK_METHOD(void,
              ObserveFieldVisibility,
              (const FieldGlobalId&,
               mojo::PendingRemote<mojom::AutofillVisibilityObserver>),
              (override));
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  ~MockAutofillClient() override = default;

  MOCK_METHOD(AutofillManager*,
              GetAutofillManagerForPrimaryMainFrame,
              (),
              (override));

  AutofillDriverRouter& router() { return router_; }

 private:
  AutofillDriverRouter router_;
};

class OmniboxAutofillDelegateTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<MockAutofillClient,
                                                 MockAutofillDriver> {
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

    payments_autofill_client().set_payments_network_interface(
        std::make_unique<payments::TestPaymentsNetworkInterface>(
            autofill_client().GetURLLoaderFactory(),
            autofill_client().GetIdentityManager(),
            &autofill_client().GetPersonalDataManager()));
    payments_autofill_client().set_multiple_request_payments_network_interface(
        std::make_unique<payments::MockMultipleRequestPaymentsNetworkInterface>(
            autofill_client().GetURLLoaderFactory(),
            *autofill_client().GetIdentityManager()));

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
                                   /*removed_forms=*/{},
                                   AutofillManagerTestApi::pass_key());
  }

  void AutofillForm(const FormData& form, const CreditCard& credit_card) {
    // Filling should trigger on the "Card Number" field located at index 1.
    autofill_manager().FillOrPreviewForm(
        mojom::ActionPersistence::kFill, form.global_id(),
        form.fields()[1].global_id(), &credit_card,
        AutofillTriggerSource::kOmniboxAutofill,
        /*blocked_fields=*/{});
  }

  void FormSubmitted(const FormData& form) {
    autofill_manager().OnFormSubmitted(form,
                                       mojom::SubmissionSource::FORM_SUBMISSION,
                                       AutofillManagerTestApi::pass_key());
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

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility);

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

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility).Times(0);

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

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility).Times(0);

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

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility).Times(0);

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

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility).Times(0);

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

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility).Times(0);

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

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility);

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

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility);

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kSuccess, 1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_MissingCreditCardNumberField_Aborts) {
  base::HistogramTester histogram_tester;

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility).Times(0);

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

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility).Times(0);

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

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility);

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

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility).Times(0);

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

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility).Times(0);

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

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility).Times(0);

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
    EXPECT_CALL(autofill_driver(), ObserveFieldVisibility);
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
    EXPECT_CALL(autofill_driver(), ObserveFieldVisibility).Times(0);
    FormsSeen({form});
    histogram_tester.ExpectTotalCount(
        "Autofill.OmniboxAutofill.ShowChipDecisionPart1", 0);
  }
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldTypesDetermined_IntersectionObserverVisible_ShowsChip) {
  FormData form = CreateTestCreditCardFormData();

  EXPECT_CALL(autofill_driver(), ObserveFieldVisibility)
      .WillOnce(
          [](const FieldGlobalId&,
             mojo::PendingRemote<mojom::AutofillVisibilityObserver> observer) {
            mojo::Remote<mojom::AutofillVisibilityObserver> remote(
                std::move(observer));
            remote->OnFieldBecameVisible();
            remote.FlushForTesting();
          });

  FormsSeen({form});

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnAutofillManagerStateChanged_WasActive_HideChip) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnFieldBecameVisible();

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());

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
  base::HistogramTester histogram_tester;
  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);
  delegate->OnAutofillManagerStateChanged(
      autofill_manager(), /*previous=*/AutofillManager::LifecycleState::kActive,
      /*current=*/AutofillManager::LifecycleState::kInactive);

  // Verify that `Reset()` recorded that the field never became visible.
  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart2",
      OmniboxAutofillShowChipDecisionPart2::
          kIntersectionObserverNeverReportedVisibility,
      1);

  // Verify that the state was reset. If it was reset, we can find the candidate
  // form again and it will log success.
  FormsSeen({form});
  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kSuccess, 1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnAutofillManagerStateChanged_WasNotActive_DoesNotHideChip) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  payments_autofill_client().ShowExpandedOmniboxAutofillChip(
      /*suggestions=*/{},
      /*on_chip_shown=*/base::DoNothing(),
      /*on_suggestions_shown=*/base::DoNothing(),
      /*on_suggestions_hidden=*/base::DoNothing(),
      /*did_select_suggestion=*/base::DoNothing(),
      /*did_deselect_suggestion=*/base::DoNothing(),
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

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnFieldBecameVisible();

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());

  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{form.global_id()},
                                 AutofillManagerTestApi::pass_key());

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
  base::HistogramTester histogram_tester;
  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{form.global_id()},
                                 AutofillManagerTestApi::pass_key());

  // Verify that `Reset()` recorded that the field never became visible.
  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart2",
      OmniboxAutofillShowChipDecisionPart2::
          kIntersectionObserverNeverReportedVisibility,
      1);

  // Verify that the state was reset. If it was reset, we can find the candidate
  // form again.
  FormsSeen({form});
  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart1",
      OmniboxAutofillShowChipDecisionPart1::kSuccess, 1);
}

TEST_F(OmniboxAutofillDelegateTest,
       OnAfterFormsSeen_FormNotRemoved_DoesNotHideChip) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  payments_autofill_client().ShowExpandedOmniboxAutofillChip(
      /*suggestions=*/{},
      /*on_chip_shown=*/base::DoNothing(),
      /*on_suggestions_shown=*/base::DoNothing(),
      /*on_suggestions_hidden=*/base::DoNothing(),
      /*did_select_suggestion=*/base::DoNothing(),
      /*did_deselect_suggestion=*/base::DoNothing(),
      /*did_accept_suggestion=*/base::DoNothing());

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());

  FormGlobalId different_form_id = test::MakeFormGlobalId();
  ASSERT_NE(different_form_id, form.global_id());
  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{different_form_id},
                                 AutofillManagerTestApi::pass_key());

  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());
  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnAfterFormsSeen_CandidateFormNotFound_ReturnsEarly) {
  FormGlobalId form_id = test::MakeFormGlobalId();
  autofill_manager().OnFormsSeen(/*updated_forms=*/{},
                                 /*removed_forms=*/{form_id},
                                 AutofillManagerTestApi::pass_key());

  // Since a candidate form has not been found yet (`candidate_form_found_` is
  // false), checking `form_id` against the uninitialized
  // `trigger_form_global_id_` would be pointless. Instead, abort hide logic.
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnAfterDidAutofillForm_CandidateFormNotFound_ReturnsEarly) {
  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnAfterDidAutofillForm(autofill_manager(),
                                   test::MakeFormGlobalId());

  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnAfterDidAutofillForm_TriggerFieldNotVisible_ReturnsEarly) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnAfterDidAutofillForm(autofill_manager(), form.global_id());

  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnAfterDidAutofillForm_DifferentForm_ReturnsEarly) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnFieldBecameVisible();

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());

  FormGlobalId different_form_id = test::MakeFormGlobalId();
  ASSERT_NE(different_form_id, form.global_id());
  delegate->OnAfterDidAutofillForm(autofill_manager(), different_form_id);

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());
}

TEST_F(OmniboxAutofillDelegateTest, OnAfterDidAutofillForm_HidesChip) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnFieldBecameVisible();

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_hidden());

  delegate->OnAfterDidAutofillForm(autofill_manager(), form.global_id());

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_hidden());
  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldBecameVisible_NoAutofillManager_ReturnsEarly) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  DeleteAllAutofillDrivers();

  delegate->OnFieldBecameVisible();

  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldBecameVisible_FormNotFound_ReturnsEarly) {
  // Do not call `FormsSeen` to simulate form not found.

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnFieldBecameVisible();

  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest,
       OnFieldBecameVisible_FieldNotFound_ReturnsEarly) {
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

  delegate->OnFieldBecameVisible();

  EXPECT_FALSE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest, OnFieldBecameVisible_ShowsChip) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnFieldBecameVisible();

  EXPECT_TRUE(payments_autofill_client().omnibox_autofill_chip_shown());
}

TEST_F(OmniboxAutofillDelegateTest, OnFieldBecameVisible_LogsMetrics) {
  base::HistogramTester histogram_tester;

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnFieldBecameVisible();

  // Verify metrics logging. `SetUp()` adds 1 credit card, so count should be 1.
  histogram_tester.ExpectUniqueSample("Autofill.SuggestionsCount.CreditCard", 1,
                                      1);
  histogram_tester.ExpectUniqueSample("Autofill.QueriedCreditCardFormIsSecure",
                                      true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.OmniboxAutofill.ShowChipDecisionPart2",
      OmniboxAutofillShowChipDecisionPart2::kSuccess, 1);
}

TEST_F(OmniboxAutofillDelegateTest, OnChipShown_LogsMetrics) {
  base::HistogramTester histogram_tester;

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  delegate->OnChipShown();

  histogram_tester.ExpectBucketCount("Autofill.OmniboxAutofill.Events",
                                     OmniboxAutofillEvents::kChipShown, 1);
  histogram_tester.ExpectBucketCount("Autofill.OmniboxAutofill.Events",
                                     OmniboxAutofillEvents::kChipShownOnce, 1);
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
  test_api(autofill_manager()).Reset();

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
  test_api(autofill_manager()).Reset();

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

TEST_F(OmniboxAutofillDelegateTest,
       OnSuggestionsShown_LogOmniboxAutofillEventMetrics) {
  base::HistogramTester histogram_tester;

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry)};

  delegate->OnSuggestionsShown(suggestions, std::nullopt);

  histogram_tester.ExpectBucketCount("Autofill.OmniboxAutofill.Events",
                                     OmniboxAutofillEvents::kChipClicked, 1);
  histogram_tester.ExpectBucketCount("Autofill.OmniboxAutofill.Events",
                                     OmniboxAutofillEvents::kChipClickedOnce,
                                     1);
}

TEST_F(OmniboxAutofillDelegateTest, OnSuggestionsHidden_ForwardToObserver) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

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

TEST_F(OmniboxAutofillDelegateTest, ClearPreviewedForm) {
  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  EXPECT_CALL(autofill_driver(), RendererShouldClearPreviewedForm);
  delegate->ClearPreviewedForm();
}

TEST_F(OmniboxAutofillDelegateTest,
       DidAcceptSuggestion_LogOmniboxAutofillEventMetrics) {
  base::HistogramTester histogram_tester;

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  Suggestion suggestion(SuggestionType::kCreditCardEntry);

  delegate->DidAcceptSuggestion(suggestion, /*metadata=*/{});

  histogram_tester.ExpectBucketCount("Autofill.OmniboxAutofill.Events",
                                     OmniboxAutofillEvents::kSuggestionAccepted,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.OmniboxAutofill.Events",
      OmniboxAutofillEvents::kSuggestionAcceptedOnce, 1);
}

TEST_F(OmniboxAutofillDelegateTest, FormFilled_LogOmniboxAutofillEventMetrics) {
  base::HistogramTester histogram_tester;

  // Add local credit card.
  CreditCard local_card = test::GetCreditCard();
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .AddCreditCard(local_card);

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  // Fill the form synchronously using the local card.
  AutofillForm(form, local_card);

  histogram_tester.ExpectBucketCount("Autofill.OmniboxAutofill.Events",
                                     OmniboxAutofillEvents::kFormFilled, 1);
  histogram_tester.ExpectBucketCount("Autofill.OmniboxAutofill.Events",
                                     OmniboxAutofillEvents::kFormFilledOnce, 1);
}

TEST_F(OmniboxAutofillDelegateTest,
       FormSubmitted_LogOmniboxAutofillEventMetrics) {
  base::HistogramTester histogram_tester;

  // Add local credit card.
  CreditCard local_card = test::GetCreditCard();
  autofill_client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .AddCreditCard(local_card);

  FormData form = CreateTestCreditCardFormData();
  FormsSeen({form});

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  // Simulate showing suggestions. This marks form as interacted, which is a
  // prerequisite for logging submission metrics.
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kCreditCardEntry)};
  delegate->OnSuggestionsShown(suggestions, std::nullopt);

  // Fill the form synchronously using the local card.
  AutofillForm(form, local_card);

  // Submit the form.
  FormSubmitted(form);

  histogram_tester.ExpectBucketCount("Autofill.OmniboxAutofill.Events",
                                     OmniboxAutofillEvents::kFormSubmittedOnce,
                                     1);
}

class RoutingMockAutofillDriver : public TestAutofillDriver {
 public:
  using TestAutofillDriver::TestAutofillDriver;
  RoutingMockAutofillDriver(const RoutingMockAutofillDriver&) = delete;
  RoutingMockAutofillDriver& operator=(const RoutingMockAutofillDriver&) =
      delete;

  ~RoutingMockAutofillDriver() override {
    router().UnregisterDriver(*this, /*driver_is_dying=*/true);
  }

  base::flat_set<FieldGlobalId> ApplyFormAction(
      mojom::FormActionType action_type,
      mojom::ActionPersistence action_persistence,
      base::span<const FormFieldData> fields,
      const FillId& fill_id,
      bool supports_refill,
      const url::Origin& triggered_origin,
      const absl::flat_hash_map<FieldGlobalId, FieldType>& field_type_map,
      const Section& section_for_clear_form_on_ios) override {
    url::Origin main_origin =
        GetAutofillClient().GetLastCommittedPrimaryMainFrameOrigin();
    return router().ApplyFormAction(
        [](AutofillDriver& target, mojom::FormActionType action_type,
           mojom::ActionPersistence action_persistence,
           const std::vector<FormFieldData::FillData>& fields,
           const FillId& fill_id, bool supports_refill) {
          static_cast<RoutingMockAutofillDriver&>(target)
              .ExecuteApplyFormAction(action_type, action_persistence);
        },
        action_type, action_persistence, fields, fill_id, supports_refill,
        main_origin, triggered_origin, field_type_map);
  }

  MOCK_METHOD(void,
              ExecuteApplyFormAction,
              (mojom::FormActionType action_type,
               mojom::ActionPersistence action_persistence));

  AutofillDriverRouter& router() {
    return static_cast<MockAutofillClient&>(GetAutofillClient()).router();
  }
};

class OmniboxAutofillDelegateFillingTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<MockAutofillClient,
                                                 RoutingMockAutofillDriver> {
 public:
  OmniboxAutofillDelegateFillingTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kAutofillEnableOmniboxAutofill);
  }

  ~OmniboxAutofillDelegateFillingTest() override = default;

  void SetUp() override {
    InitAutofillClient();

    payments_autofill_client().set_multiple_request_payments_network_interface(
        std::make_unique<
            NiceMock<payments::MockMultipleRequestPaymentsNetworkInterface>>(
            autofill_client().GetURLLoaderFactory(),
            *autofill_client().GetIdentityManager()));

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
        .AddCreditCard(test::GetCreditCard());
  }

  void TearDown() override { DestroyAutofillClient(); }

  FormData CreateTestCreditCardFormData(AutofillDriver& driver) {
    FormData form;
    FormRendererId form_id = test::MakeFormRendererId();
    form.set_renderer_id(form_id);
    form.set_name(u"MyForm");
    form.set_url(GURL("https://myform.com/form.html"));
    form.set_action(GURL("https://myform.com/submit.html"));
    autofill_client().set_last_committed_primary_main_frame_url(form.url());

    auto AppendField = [&](FormFieldData field) {
      field.set_host_form_id(form_id);
      test_api(form).Append(std::move(field));
    };

    AppendField(CreateTestFormField("Name on Card", "nameoncard", "",
                                    FormControlType::kInputText));
    AppendField(CreateTestFormField("Card Number", "cardnumber", "",
                                    FormControlType::kInputText));
    AppendField(CreateTestFormField("Expiration Date", "ccmonth", "",
                                    FormControlType::kInputText));
    AppendField(
        CreateTestFormField("", "ccyear", "", FormControlType::kInputText));
    AppendField(
        CreateTestFormField("CVC", "cvc", "", FormControlType::kInputText));

    return CreateFormDataForFrame(form, driver.GetFrameToken());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that selecting and accepting a credit card suggestion for a subframe
// credit card form routes the preview and fill actions to the subframe driver.
TEST_F(OmniboxAutofillDelegateFillingTest, FillOrPreviewCard_SubframeForm) {
  // Create drivers and set unique frame tokens.
  CreateAutofillDriver();  // Main frame (index 0)
  CreateAutofillDriver();  // Subframe (index 1)

  autofill_driver(0).SetLocalFrameToken(test::MakeLocalFrameToken());
  autofill_driver(1).SetLocalFrameToken(test::MakeLocalFrameToken());

  // Establish the subframe relationship.
  autofill_driver(1).SetParent(&autofill_driver(0));
  autofill_driver(1).SetIsEmbedded(true);
  autofill_driver(0).SetParent(nullptr);
  autofill_driver(0).SetIsEmbedded(false);
  autofill_driver(0).SetIsActive(true);
  autofill_driver(1).SetIsActive(true);

  ON_CALL(autofill_client(), GetAutofillManagerForPrimaryMainFrame)
      .WillByDefault(Return(&autofill_manager(0)));

  // Allow the subframe URL in the optimization guide decider.
  ON_CALL(*autofill_client().GetAutofillOptimizationGuideDecider(),
          IsUrlEligibleForOmniboxAutofill)
      .WillByDefault(Return(true));

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  // Create parent form in main frame.
  FormData parent_form;
  FormRendererId parent_form_id = test::MakeFormRendererId();
  parent_form.set_renderer_id(parent_form_id);
  parent_form.set_name(u"ParentForm");
  parent_form.set_url(GURL("https://myform.com/form.html"));
  parent_form.set_action(GURL("https://myform.com/submit.html"));
  autofill_client().set_last_committed_primary_main_frame_url(
      parent_form.url());

  // Add a dummy field to parent form.
  FormFieldData parent_field = CreateTestFormField(
      "Parent Field", "parentfield", "", FormControlType::kInputText);
  parent_field.set_host_form_id(parent_form_id);
  test_api(parent_form).Append(std::move(parent_field));

  // Connect child frame.
  FrameTokenWithPredecessor child_frame;
  child_frame.token = autofill_driver(1).GetFrameToken();
  child_frame.predecessor = 0;  // after the first field
  parent_form.set_child_frames({child_frame});

  parent_form =
      CreateFormDataForFrame(parent_form, autofill_driver(0).GetFrameToken());

  // Create child form in subframe (the credit card form).
  FormData child_form = CreateTestCreditCardFormData(autofill_driver(1));

  // Register parent form in main manager.
  autofill_driver(0).router().FormsSeen(
      [](AutofillDriver& target, std::vector<FormData> forms,
         std::vector<FormGlobalId> removed) {
        target.GetAutofillManager().OnFormsSeen(
            forms, removed, AutofillManagerTestApi::pass_key());
      },
      autofill_driver(0), {parent_form}, {});

  // Register child form in subframe manager.
  // This triggers flattening, which fires OnFieldTypesDetermined from the main
  // manager.
  autofill_driver(1).router().FormsSeen(
      [](AutofillDriver& target, std::vector<FormData> forms,
         std::vector<FormGlobalId> removed) {
        target.GetAutofillManager().OnFormsSeen(
            forms, removed, AutofillManagerTestApi::pass_key());
      },
      autofill_driver(1), {child_form}, {});

  // Expect routed preview on subframe driver (1), not main (0).
  EXPECT_CALL(autofill_driver(1),
              ExecuteApplyFormAction(mojom::FormActionType::kFill,
                                     mojom::ActionPersistence::kPreview))
      .Times(1);
  EXPECT_CALL(autofill_driver(0), ExecuteApplyFormAction).Times(0);

  // Trigger preview (DidSelectSuggestion).
  Suggestion suggestion(SuggestionType::kCreditCardEntry);
  const std::vector<const CreditCard*>& cards = autofill_client()
                                                    .GetPersonalDataManager()
                                                    .payments_data_manager()
                                                    .GetCreditCards();
  ASSERT_EQ(cards.size(), 1u);
  std::string guid = cards[0]->guid();
  suggestion.payload = Suggestion::Guid(guid);

  delegate->DidSelectSuggestion(suggestion);

  // Trigger fill (DidAcceptSuggestion).
  EXPECT_CALL(autofill_driver(1),
              ExecuteApplyFormAction(mojom::FormActionType::kFill,
                                     mojom::ActionPersistence::kFill))
      .Times(1);
  EXPECT_CALL(autofill_driver(0), ExecuteApplyFormAction).Times(0);

  delegate->DidAcceptSuggestion(suggestion, {});
}

// Tests that selecting and accepting a credit card suggestion for a main frame
// credit card form routes the preview and fill actions to the main frame
// driver.
TEST_F(OmniboxAutofillDelegateFillingTest, FillOrPreviewCard_MainFrameForm) {
  // Create only the main frame driver.
  CreateAutofillDriver();  // Main frame (index 0)
  autofill_driver(0).SetLocalFrameToken(test::MakeLocalFrameToken());
  autofill_driver(0).SetParent(nullptr);
  autofill_driver(0).SetIsEmbedded(false);
  autofill_driver(0).SetIsActive(true);

  ON_CALL(autofill_client(), GetAutofillManagerForPrimaryMainFrame)
      .WillByDefault(Return(&autofill_manager(0)));

  OmniboxAutofillDelegate* delegate =
      payments_autofill_client().GetOmniboxAutofillDelegate();
  ASSERT_TRUE(delegate);

  // Create credit card form in main frame.
  FormData form = CreateTestCreditCardFormData(autofill_driver(0));
  autofill_client().set_last_committed_primary_main_frame_url(form.url());

  // Register form in main manager.
  autofill_driver(0).router().FormsSeen(
      [](AutofillDriver& target, std::vector<FormData> forms,
         std::vector<FormGlobalId> removed) {
        target.GetAutofillManager().OnFormsSeen(
            forms, removed, AutofillManagerTestApi::pass_key());
      },
      autofill_driver(0), {form}, {});

  // Expect preview on main driver (0).
  EXPECT_CALL(autofill_driver(0),
              ExecuteApplyFormAction(mojom::FormActionType::kFill,
                                     mojom::ActionPersistence::kPreview))
      .Times(1);

  // Trigger preview (DidSelectSuggestion).
  Suggestion suggestion(SuggestionType::kCreditCardEntry);
  const std::vector<const CreditCard*>& cards = autofill_client()
                                                    .GetPersonalDataManager()
                                                    .payments_data_manager()
                                                    .GetCreditCards();
  ASSERT_EQ(cards.size(), 1u);
  std::string guid = cards[0]->guid();
  suggestion.payload = Suggestion::Guid(guid);

  delegate->DidSelectSuggestion(suggestion);

  // Trigger fill (DidAcceptSuggestion).
  EXPECT_CALL(autofill_driver(0),
              ExecuteApplyFormAction(mojom::FormActionType::kFill,
                                     mojom::ActionPersistence::kFill))
      .Times(1);

  delegate->DidAcceptSuggestion(suggestion, {});
}

}  // namespace
}  // namespace autofill
