// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_churned_users_manager.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

class MockPaymentsAutofillClient : public TestPaymentsAutofillClient {
 public:
  explicit MockPaymentsAutofillClient(AutofillClient* client)
      : TestPaymentsAutofillClient(client) {}
  ~MockPaymentsAutofillClient() override = default;

  MOCK_METHOD(void, ShowPaymentsChurnedUsersUI, (), (override));
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() {
    set_payments_autofill_client(
        std::make_unique<MockPaymentsAutofillClient>(this));
  }
};

class PaymentsChurnedUsersManagerTest
    : public testing::Test,
      public WithTestAutofillClientDriverManager<MockAutofillClient,
                                                 TestAutofillDriver,
                                                 TestBrowserAutofillManager> {
 public:
  void SetUp() override {
    InitAutofillClient();
    CreateAutofillDriver();
  }

  MockPaymentsAutofillClient* payments_client() {
    return static_cast<MockPaymentsAutofillClient*>(
        autofill_client().GetPaymentsAutofillClient());
  }

  void SimulateOnFieldTypesDetermined(bool is_credit_card_form,
                                      bool is_visible = true) {
    FormData form_data = test::GetFormData(
        is_credit_card_form
            ? std::vector<FieldType>{CREDIT_CARD_NAME_FULL, CREDIT_CARD_NUMBER,
                                     CREDIT_CARD_EXP_MONTH,
                                     CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
                                     CREDIT_CARD_VERIFICATION_CODE}
            : std::vector<FieldType>{NAME_FIRST, NAME_LAST, ADDRESS_HOME_LINE1,
                                     ADDRESS_HOME_ZIP});

    std::vector<FormFieldData> fields = form_data.ExtractFields();
    for (FormFieldData& field : fields) {
      field.set_is_visible(is_visible);
    }
    form_data.set_fields(std::move(fields));

    autofill_manager().OnFormsSeen({form_data}, {});
    task_environment_.RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<PaymentsChurnedUsersManager> manager_;
};

// Tests that the Payments Churned Users UI is shown when all conditions for
// showing are met.
TEST_F(PaymentsChurnedUsersManagerTest, ShowUITriggered) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  EXPECT_CALL(*payments_client(), ShowPaymentsChurnedUsersUI());
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true);
}

// Tests that the Payments Churned Users UI is not shown if the feature flag is
// off.
TEST_F(PaymentsChurnedUsersManagerTest, FeatureFlagOff_ShowUINotTriggered) {
  feature_list_.InitAndDisableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  EXPECT_CALL(*payments_client(), ShowPaymentsChurnedUsersUI()).Times(0);
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true);
}

// Tests that the Payments Churned Users UI is not shown if the user did not
// turn off the credit card enabled pref (for example, if an extension turned it
// off instead).
TEST_F(PaymentsChurnedUsersManagerTest,
       PrefNotUserControlled_ShowUINotTriggered) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->ClearPref(prefs::kAutofillCreditCardEnabled);

  EXPECT_CALL(*payments_client(), ShowPaymentsChurnedUsersUI()).Times(0);
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true);
}

// Tests that the Payments Churned Users UI is not shown if the parsed form is
// not a credit card form.
TEST_F(PaymentsChurnedUsersManagerTest, NotCreditCardForm_ShowUINotTriggered) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  EXPECT_CALL(*payments_client(), ShowPaymentsChurnedUsersUI()).Times(0);
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/false);
}

// Tests that the Payments Churned Users UI is not shown if the credit card form
// is not visible.
TEST_F(PaymentsChurnedUsersManagerTest,
       NotVisibleCreditCardForm_ShowUINotTriggered) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  EXPECT_CALL(*payments_client(), ShowPaymentsChurnedUsersUI()).Times(0);
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true,
                                 /*is_visible=*/false);
}

// Tests that the Payments Churned Users UI is not shown if the credit card
// enabled pref is already turned on.
TEST_F(PaymentsChurnedUsersManagerTest, PrefAlreadyEnabled_ShowUINotTriggered) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           true);

  EXPECT_CALL(*payments_client(), ShowPaymentsChurnedUsersUI()).Times(0);
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true);
}

}  // namespace
}  // namespace autofill::payments
