// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_churned_users_manager.h"

#include "base/functional/callback.h"
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
#include "components/autofill/core/browser/strike_databases/payments/payments_churned_users_strike_database.h"
#include "components/autofill/core/browser/strike_databases/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/strike_database/strike_database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill::payments {
namespace {

class MockPaymentsAutofillClient : public TestPaymentsAutofillClient {
 public:
  explicit MockPaymentsAutofillClient(AutofillClient* client)
      : TestPaymentsAutofillClient(client) {}
  ~MockPaymentsAutofillClient() override = default;

  MOCK_METHOD(void,
              ShowPaymentsChurnedUsersUI,
              (base::OnceClosure, base::OnceClosure, base::OnceClosure),
              (override));
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() {
    set_payments_autofill_client(
        std::make_unique<MockPaymentsAutofillClient>(this));
    set_test_strike_database(std::make_unique<TestStrikeDatabase>());
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

    autofill_manager().OnFormsSeen({form_data}, {},
                                   AutofillManagerTestApi::pass_key());
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
TEST_F(PaymentsChurnedUsersManagerTest, ShowUiTriggered) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  EXPECT_CALL(*payments_client(),
              ShowPaymentsChurnedUsersUI(testing::_, testing::_, testing::_));
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true);
}

// Tests that the Payments Churned Users UI's accept callback turns on the
// autofill credit card enabled pref.
TEST_F(PaymentsChurnedUsersManagerTest, AcceptCallbackTurnsOnPref) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  base::OnceClosure accept_callback;
  EXPECT_CALL(*payments_client(),
              ShowPaymentsChurnedUsersUI(testing::_, testing::_, testing::_))
      .WillOnce([&](base::OnceClosure accept, base::OnceClosure cancel,
                    base::OnceClosure closed) {
        accept_callback = std::move(accept);
      });
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true);

  ASSERT_FALSE(autofill_client().GetPrefs()->GetBoolean(
      prefs::kAutofillCreditCardEnabled));
  ASSERT_TRUE(accept_callback);
  std::move(accept_callback).Run();
  EXPECT_TRUE(autofill_client().GetPrefs()->GetBoolean(
      prefs::kAutofillCreditCardEnabled));
}

// Tests that the Payments Churned Users UI is not shown if the user is off the
// record.
TEST_F(PaymentsChurnedUsersManagerTest, OffTheRecord_ShowUiNotTriggered) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);
  autofill_client().set_is_off_the_record(true);

  EXPECT_CALL(*payments_client(), ShowPaymentsChurnedUsersUI).Times(0);
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true);
}

// Tests that the Payments Churned Users UI is not shown if the feature flag is
// off.
TEST_F(PaymentsChurnedUsersManagerTest, FeatureFlagOff_ShowUiNotTriggered) {
  feature_list_.InitAndDisableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  EXPECT_CALL(*payments_client(),
              ShowPaymentsChurnedUsersUI(testing::_, testing::_, testing::_))
      .Times(0);
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true);
}

// Tests that the Payments Churned Users UI is not shown if the user did not
// turn off the credit card enabled pref (for example, if an extension turned it
// off instead).
TEST_F(PaymentsChurnedUsersManagerTest,
       PrefNotUserControlled_ShowUiNotTriggered) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->ClearPref(prefs::kAutofillCreditCardEnabled);

  EXPECT_CALL(*payments_client(),
              ShowPaymentsChurnedUsersUI(testing::_, testing::_, testing::_))
      .Times(0);
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true);
}

// Tests that the Payments Churned Users UI is not shown if the parsed form is
// not a credit card form.
TEST_F(PaymentsChurnedUsersManagerTest, NotCreditCardForm_ShowUiNotTriggered) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  EXPECT_CALL(*payments_client(),
              ShowPaymentsChurnedUsersUI(testing::_, testing::_, testing::_))
      .Times(0);
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/false);
}

// Tests that the Payments Churned Users UI is not shown if the credit card form
// is not visible.
TEST_F(PaymentsChurnedUsersManagerTest,
       NotVisibleCreditCardForm_ShowUiNotTriggered) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  EXPECT_CALL(*payments_client(),
              ShowPaymentsChurnedUsersUI(testing::_, testing::_, testing::_))
      .Times(0);
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true,
                                 /*is_visible=*/false);
}

// Tests that the Payments Churned Users UI is not shown if the credit card
// enabled pref is already turned on.
TEST_F(PaymentsChurnedUsersManagerTest, PrefAlreadyEnabled_ShowUiNotTriggered) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           true);

  EXPECT_CALL(*payments_client(),
              ShowPaymentsChurnedUsersUI(testing::_, testing::_, testing::_))
      .Times(0);
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true);
}

// Tests that the Payments Churned Users UI is not shown if the user has reached
// the maximum number of strikes.
TEST_F(PaymentsChurnedUsersManagerTest, ShowUiNotTriggered_MaxStrikesReached) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  PaymentsChurnedUsersStrikeDatabase strike_database(
      autofill_client().GetStrikeDatabase());
  strike_database.AddStrikes(strike_database.GetMaxStrikesLimit());

  EXPECT_CALL(*payments_client(),
              ShowPaymentsChurnedUsersUI(testing::_, testing::_, testing::_))
      .Times(0);
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true);
}

// Tests that cancelling the UI adds the maximum number of strikes to the
// strike database, preventing it from showing again.
TEST_F(PaymentsChurnedUsersManagerTest, CancelCallbackAddsStrikes) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  base::OnceClosure cancel_callback;
  EXPECT_CALL(*payments_client(),
              ShowPaymentsChurnedUsersUI(testing::_, testing::_, testing::_))
      .WillOnce([&](base::OnceClosure accept, base::OnceClosure cancel,
                    base::OnceClosure closed) {
        cancel_callback = std::move(cancel);
      });
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true);

  PaymentsChurnedUsersStrikeDatabase strike_database(
      autofill_client().GetStrikeDatabase());
  EXPECT_EQ(strike_database.GetStrikes(), 0);

  ASSERT_TRUE(cancel_callback);
  std::move(cancel_callback).Run();

  EXPECT_EQ(strike_database.GetStrikes(), strike_database.GetMaxStrikesLimit());
}

// Tests that closing the UI adds a single strike to the strike database.
TEST_F(PaymentsChurnedUsersManagerTest, ClosedCallbackAddsStrike) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  base::OnceClosure closed_callback;
  EXPECT_CALL(*payments_client(),
              ShowPaymentsChurnedUsersUI(testing::_, testing::_, testing::_))
      .WillOnce([&](base::OnceClosure accept, base::OnceClosure cancel,
                    base::OnceClosure closed) {
        closed_callback = std::move(closed);
      });
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true);

  PaymentsChurnedUsersStrikeDatabase strike_database(
      autofill_client().GetStrikeDatabase());
  EXPECT_EQ(strike_database.GetStrikes(), 0);

  ASSERT_TRUE(closed_callback);
  std::move(closed_callback).Run();

  EXPECT_EQ(strike_database.GetStrikes(), 1);
}

// Tests that accepting the UI clears any existing strikes from the strike
// database.
TEST_F(PaymentsChurnedUsersManagerTest, AcceptCallbackClearsStrikes) {
  feature_list_.InitAndEnableFeature(
      features::kAutofillEnableResurrectingPaymentsUsers);
  manager_ = std::make_unique<PaymentsChurnedUsersManager>(&autofill_client());

  autofill_client().GetPrefs()->SetBoolean(prefs::kAutofillCreditCardEnabled,
                                           false);

  PaymentsChurnedUsersStrikeDatabase strike_database(
      autofill_client().GetStrikeDatabase());
  strike_database.AddStrikes(strike_database.GetMaxStrikesLimit() - 1);
  EXPECT_EQ(strike_database.GetStrikes(),
            strike_database.GetMaxStrikesLimit() - 1);
  task_environment_.FastForwardBy(base::Days(8));

  base::OnceClosure accept_callback;
  EXPECT_CALL(*payments_client(),
              ShowPaymentsChurnedUsersUI(testing::_, testing::_, testing::_))
      .WillOnce([&](base::OnceClosure accept, base::OnceClosure cancel,
                    base::OnceClosure closed) {
        accept_callback = std::move(accept);
      });
  SimulateOnFieldTypesDetermined(/*is_credit_card_form=*/true);

  ASSERT_TRUE(accept_callback);
  std::move(accept_callback).Run();

  EXPECT_EQ(strike_database.GetStrikes(), 0);
}

}  // namespace
}  // namespace autofill::payments
