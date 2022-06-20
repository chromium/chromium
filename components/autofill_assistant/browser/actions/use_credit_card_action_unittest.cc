// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_credit_card_action.h"

#include <utility>

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/string_conversions_util.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {
const char kFakeSelector[] = "#selector";
const char kFakeCvc[] = "123";
const char16_t kFakeCvc16[] = u"123";
const char kModelIdentifier[] = "identifier";
const char kCardName[] = "Adam West";
const char kCardNumber[] = "4111111111111111";
const char kExpirationMonth[] = "9";
const char kExpirationYear[] = "2050";

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArgPointee;

class UseCreditCardActionTest : public testing::Test {
 public:
  void SetUp() override {
    autofill::test::SetCreditCardInfo(&credit_card_, kCardName, kCardNumber,
                                      kExpirationMonth, kExpirationYear,
                                      /* billing_address_id= */ "");

    // Store copies of |credit_card_| in |user_data_| and |user_model_|.
    user_model_.SetSelectedCreditCard(
        std::make_unique<autofill::CreditCard>(credit_card_), &user_data_);
    auto cards =
        std::make_unique<std::vector<std::unique_ptr<autofill::CreditCard>>>();
    cards->emplace_back(std::make_unique<autofill::CreditCard>(credit_card_));
    user_model_.SetAutofillCreditCards(std::move(cards));
    ValueProto card_value;
    card_value.mutable_credit_cards()->add_values()->set_guid(
        credit_card_.guid());
    user_model_.SetValue(kModelIdentifier, card_value);

    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));
    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, GetUserModel)
        .WillByDefault(Return(&user_model_));
    ON_CALL(mock_action_delegate_, GetPersonalDataManager)
        .WillByDefault(Return(&mock_personal_data_manager_));
    ON_CALL(mock_action_delegate_, RunElementChecks)
        .WillByDefault([this](BatchElementChecker* checker) {
          checker->Run(&mock_web_controller_);
        });
    ON_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));
    ON_CALL(mock_action_delegate_, GetFullCard)
        .WillByDefault(
            [](const autofill::CreditCard* credit_card,
               base::OnceCallback<void(const ClientStatus&,
                                       std::unique_ptr<autofill::CreditCard>,
                                       const std::u16string&)> callback) {
              std::move(callback).Run(
                  credit_card ? OkClientStatus()
                              : ClientStatus(GET_FULL_CARD_FAILED),
                  credit_card
                      ? std::make_unique<autofill::CreditCard>(*credit_card)
                      : nullptr,
                  kFakeCvc16);
            });
    test_util::MockFindAnyElement(mock_web_controller_);
  }

 protected:
  ActionProto CreateUseCreditCardAction() {
    ActionProto action;
    *action.mutable_use_card()->mutable_form_field_element() =
        ToSelectorProto(kFakeSelector);
    return action;
  }

  RequiredFieldProto* AddRequiredField(ActionProto* action,
                                       int key,
                                       const std::string& selector) {
    auto* required_field = action->mutable_use_card()->add_required_fields();
    required_field->mutable_value_expression()->add_chunk()->set_key(key);
    *required_field->mutable_element() = ToSelectorProto(selector);
    return required_field;
  }

  ActionProto CreateUseCardAction() {
    ActionProto action;
    UseCreditCardProto* use_card = action.mutable_use_card();
    *use_card->mutable_form_field_element() = ToSelectorProto(kFakeSelector);
    return action;
  }

  ProcessedActionStatusProto ProcessAction(const ActionProto& action_proto) {
    UseCreditCardAction action(&mock_action_delegate_, action_proto);
    ProcessedActionProto capture;
    EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&capture));
    action.ProcessAction(callback_.Get());
    return capture.status();
  }

  base::MockCallback<Action::ProcessActionCallback> callback_;
  MockPersonalDataManager mock_personal_data_manager_;
  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  UserData user_data_;
  UserModel user_model_;
  Selector fake_selector_ = Selector({kFakeSelector});
  autofill::CreditCard credit_card_ = {base::GenerateGUID(),
                                       autofill::test::kEmptyOrigin};
};

TEST_F(UseCreditCardActionTest, InvalidActionNoSelectorSet) {
  ActionProto action;
  action.mutable_use_card();
  EXPECT_EQ(ProcessedActionStatusProto::INVALID_ACTION, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest,
       InvalidActionSkipAutofillWithoutRequiredFields) {
  ActionProto action;
  auto* use_card = action.mutable_use_card();
  use_card->set_skip_autofill(true);
  EXPECT_EQ(ProcessedActionStatusProto::INVALID_ACTION, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, InvalidActionSkipResolveWithoutRequiredFields) {
  ActionProto action;
  auto* use_card = action.mutable_use_card();
  use_card->set_skip_resolve(true);
  EXPECT_EQ(ProcessedActionStatusProto::INVALID_ACTION, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, PreconditionFailedNoCreditCardInUserData) {
  ActionProto action;
  auto* use_card = action.mutable_use_card();
  *use_card->mutable_form_field_element() = ToSelectorProto(kFakeSelector);
  user_model_.SetSelectedCreditCard(nullptr, &user_data_);
  EXPECT_EQ(ProcessedActionStatusProto::PRECONDITION_FAILED,
            ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, CreditCardInUserDataSucceeds) {
  ON_CALL(mock_action_delegate_, OnShortWaitForElement(fake_selector_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "not empty"));
  ActionProto action;
  auto* use_card = action.mutable_use_card();
  *use_card->mutable_form_field_element() = ToSelectorProto(kFakeSelector);
  EXPECT_CALL(
      mock_web_controller_,
      FillCardForm(Pointee(Eq(credit_card_)), _, std::u16string(kFakeCvc16),
                   EqualsElement(test_util::MockFindElement(
                       mock_action_delegate_, fake_selector_)),
                   _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));
  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, InvalidActionModelIdentifierSetButEmpty) {
  ActionProto action;
  auto* use_card = action.mutable_use_card();
  *use_card->mutable_form_field_element() = ToSelectorProto(kFakeSelector);
  use_card->set_model_identifier("");
  EXPECT_EQ(ProcessedActionStatusProto::INVALID_ACTION, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest,
       PreconditionFailedNoCreditCardForModelIdentifier) {
  ActionProto action;
  auto* use_card = action.mutable_use_card();
  *use_card->mutable_form_field_element() = ToSelectorProto(kFakeSelector);
  use_card->set_model_identifier("invalid");
  EXPECT_EQ(ProcessedActionStatusProto::PRECONDITION_FAILED,
            ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, CreditCardInUserModelSucceeds) {
  ON_CALL(mock_action_delegate_, OnShortWaitForElement(fake_selector_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), base::Seconds(0)));
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "not empty"));
  ActionProto action;
  auto* use_card = action.mutable_use_card();
  *use_card->mutable_form_field_element() = ToSelectorProto(kFakeSelector);
  use_card->set_model_identifier(kModelIdentifier);
  EXPECT_CALL(
      mock_web_controller_,
      FillCardForm(Pointee(Eq(credit_card_)), _, std::u16string(kFakeCvc16),
                   EqualsElement(test_util::MockFindElement(
                       mock_action_delegate_, fake_selector_)),
                   _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));
  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, FillCreditCard) {
  ActionProto action = CreateUseCreditCardAction();

  user_model_.SetSelectedCreditCard(std::make_unique<autofill::CreditCard>(),
                                    &user_data_);
  EXPECT_CALL(mock_web_controller_,
              FillCardForm(NotNull(), _, std::u16string(kFakeCvc16),
                           EqualsElement(test_util::MockFindElement(
                               mock_action_delegate_, fake_selector_)),
                           _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, FillCreditCardRequiredFieldsFilled) {
  // Validation succeeds.
  ON_CALL(mock_web_controller_, GetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  ActionProto action = CreateUseCreditCardAction();
  AddRequiredField(
      &action,
      static_cast<int>(AutofillFormatProto::CREDIT_CARD_VERIFICATION_CODE),
      "#cvc");
  AddRequiredField(
      &action,
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH),
      "#expmonth");

  user_model_.SetSelectedCreditCard(std::make_unique<autofill::CreditCard>(),
                                    &user_data_);
  EXPECT_CALL(mock_web_controller_,
              FillCardForm(NotNull(), _, std::u16string(kFakeCvc16),
                           EqualsElement(test_util::MockFindElement(
                               mock_action_delegate_, fake_selector_)),
                           _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, FillCreditCardWithFallback) {
  InSequence sequence;

  ON_CALL(mock_web_controller_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action = CreateUseCreditCardAction();
  AddRequiredField(
      &action,
      static_cast<int>(AutofillFormatProto::CREDIT_CARD_VERIFICATION_CODE),
      "#cvc");
  AddRequiredField(
      &action,
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH),
      "#expmonth");
  AddRequiredField(
      &action,
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_EXP_2_DIGIT_YEAR),
      "#expyear2");
  AddRequiredField(
      &action,
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR),
      "#expyear4");
  AddRequiredField(
      &action,
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL),
      "#card_name");
  AddRequiredField(
      &action, static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NUMBER),
      "#card_number");
  AddRequiredField(&action,
                   static_cast<int>(AutofillFormatProto::CREDIT_CARD_NETWORK),
                   "#network");

  EXPECT_CALL(mock_web_controller_,
              FillCardForm(NotNull(), _, std::u16string(kFakeCvc16),
                           EqualsElement(test_util::MockFindElement(
                               mock_action_delegate_, fake_selector_)),
                           _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  Selector cvc_selector({"#cvc"});
  Selector expiry_month_selector({"#expmonth"});
  Selector expiry_year2_selector({"#expyear2"});
  Selector expiry_year4_selector({"#expyear4"});
  Selector card_name_selector({"#card_name"});
  Selector card_number_selector({"#card_number"});
  Selector network_selector({"#network"});

  // First validation fails with an empty value, called once for each field.
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _))
      .Times(7)
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), std::string()));

  // Expect fields to be filled
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute(kFakeCvc,
                                EqualsElement(test_util::MockFindElement(
                                    mock_action_delegate_, cvc_selector)),
                                _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute("09",
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, expiry_month_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute("50",
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, expiry_year2_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute("2050",
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, expiry_year4_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute("Adam West",
                                EqualsElement(test_util::MockFindElement(
                                    mock_action_delegate_, card_name_selector)),
                                _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute("4111111111111111",
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, card_number_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute("visa",
                                EqualsElement(test_util::MockFindElement(
                                    mock_action_delegate_, network_selector)),
                                _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // After fallback, second validation succeeds.
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _))
      .Times(7)
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, ForcedFallbackWithKeystrokes) {
  InSequence sequence;

  ON_CALL(mock_web_controller_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action = CreateUseCreditCardAction();
  auto* cvc_required = AddRequiredField(
      &action,
      static_cast<int>(AutofillFormatProto::CREDIT_CARD_VERIFICATION_CODE),
      "#cvc");
  cvc_required->set_forced(true);
  cvc_required->set_fill_strategy(SIMULATE_KEY_PRESSES);
  cvc_required->set_delay_in_millisecond(1000);

  user_model_.SetSelectedCreditCard(std::make_unique<autofill::CreditCard>(),
                                    &user_data_);
  EXPECT_CALL(mock_web_controller_,
              FillCardForm(NotNull(), _, std::u16string(kFakeCvc16),
                           EqualsElement(test_util::MockFindElement(
                               mock_action_delegate_, fake_selector_)),
                           _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  // Do not check required field.
  EXPECT_CALL(mock_web_controller_, GetFieldValue(_, _)).Times(0);

  // But we still want the CVC filled, with simulated keypresses.
  Selector cvc_selector({"#cvc"});
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, cvc_selector);
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute("", EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_action_delegate_,
              WaitUntilDocumentIsInReadyState(
                  _, DOCUMENT_INTERACTIVE, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(), base::Seconds(0)));
  EXPECT_CALL(mock_web_controller_,
              ScrollIntoView(std::string(), "center", "center",
                             EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, WaitUntilElementIsStable(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus(), base::Seconds(0)));
  EXPECT_CALL(
      mock_web_controller_,
      ClickOrTapElement(ClickType::CLICK, EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_,
              SendKeyboardInput(UTF8ToUnicode(kFakeCvc), 1000,
                                EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  // The field is only checked afterwards and is not empty.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, cvc_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, SkippingAutofill) {
  InSequence sequence;

  ON_CALL(mock_web_controller_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action;
  AddRequiredField(
      &action,
      static_cast<int>(AutofillFormatProto::CREDIT_CARD_VERIFICATION_CODE),
      "#cvc");
  action.mutable_use_card()->set_skip_autofill(true);

  Selector cvc_selector({"#cvc"});

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(_, _)).Times(0);
  EXPECT_CALL(mock_web_controller_, FillCardForm(_, _, _, _, _)).Times(0);

  // First validation fails.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, cvc_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  // Fill cvc.
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute(kFakeCvc,
                                EqualsElement(test_util::MockFindElement(
                                    mock_action_delegate_, cvc_selector)),
                                _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, cvc_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, SkippingResolve) {
  InSequence sequence;

  ON_CALL(mock_web_controller_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action;
  AddRequiredField(&action, static_cast<int>(autofill::CREDIT_CARD_NAME_FULL),
                   "#name");
  action.mutable_use_card()->set_skip_resolve(true);

  Selector name_selector({"#name"});

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(_, _)).Times(0);
  EXPECT_CALL(mock_action_delegate_, GetFullCard(_, _)).Times(0);
  EXPECT_CALL(mock_web_controller_, FillCardForm(_, _, _, _, _)).Times(0);

  // First validation fails.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, name_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  // Fill name.
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute(kCardName,
                                EqualsElement(test_util::MockFindElement(
                                    mock_action_delegate_, name_selector)),
                                _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, name_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), kCardName));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, SkippingResolveCannotFillCardNumber) {
  InSequence sequence;

  ON_CALL(mock_web_controller_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action;
  AddRequiredField(&action, static_cast<int>(autofill::CREDIT_CARD_NUMBER),
                   "#number");
  action.mutable_use_card()->set_skip_resolve(true);

  Selector number_selector({"#number"});

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(_, _)).Times(0);
  EXPECT_CALL(mock_action_delegate_, GetFullCard(_, _)).Times(0);
  EXPECT_CALL(mock_web_controller_, FillCardForm(_, _, _, _, _)).Times(0);

  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, number_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_web_controller_, SetValueAttribute(_, _, _)).Times(0);

  EXPECT_EQ(ProcessedActionStatusProto::AUTOFILL_INCOMPLETE,
            ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, AutofillFailureWithoutRequiredFieldsIsFatal) {
  ActionProto action_proto = CreateUseCreditCardAction();

  user_model_.SetSelectedCreditCard(std::make_unique<autofill::CreditCard>(),
                                    &user_data_);
  EXPECT_CALL(mock_web_controller_,
              FillCardForm(NotNull(), _, std::u16string(kFakeCvc16),
                           EqualsElement(test_util::MockFindElement(
                               mock_action_delegate_, fake_selector_)),
                           _))
      .WillOnce(RunOnceCallback<4>(ClientStatus(OTHER_ACTION_STATUS)));

  ProcessedActionProto processed_action;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&processed_action));

  UseCreditCardAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(processed_action.status(),
            ProcessedActionStatusProto::OTHER_ACTION_STATUS);
  EXPECT_EQ(processed_action.status_details().ByteSizeLong(), 0u);
}

TEST_F(UseCreditCardActionTest,
       AutofillFailureWithRequiredFieldsLaunchesFallback) {
  InSequence sequence;

  ON_CALL(mock_web_controller_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action_proto = CreateUseCreditCardAction();
  AddRequiredField(
      &action_proto,
      static_cast<int>(AutofillFormatProto::CREDIT_CARD_VERIFICATION_CODE),
      "#cvc");

  Selector cvc_selector({"#cvc"});

  user_model_.SetSelectedCreditCard(std::make_unique<autofill::CreditCard>(),
                                    &user_data_);
  EXPECT_CALL(mock_web_controller_,
              FillCardForm(NotNull(), _, std::u16string(kFakeCvc16),
                           EqualsElement(test_util::MockFindElement(
                               mock_action_delegate_, fake_selector_)),
                           _))
      .WillOnce(RunOnceCallback<4>(ClientStatus(OTHER_ACTION_STATUS)));

  // First validation fails.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, cvc_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  // Fill CVC.
  EXPECT_CALL(mock_web_controller_,
              SetValueAttribute(kFakeCvc,
                                EqualsElement(test_util::MockFindElement(
                                    mock_action_delegate_, cvc_selector)),
                                _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_,
              GetFieldValue(EqualsElement(test_util::MockFindElement(
                                mock_web_controller_, cvc_selector)),
                            _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  ProcessedActionProto processed_action;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&processed_action));

  UseCreditCardAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(processed_action.status(), OTHER_ACTION_STATUS);
  EXPECT_EQ(processed_action.status_details()
                .autofill_error_info()
                .autofill_error_status(),
            OTHER_ACTION_STATUS);
}

TEST_F(UseCreditCardActionTest, FallbackForCardExpirationSucceeds) {
  InSequence sequence;

  ON_CALL(mock_web_controller_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action_proto = CreateUseCreditCardAction();
  auto* required_field = action_proto.mutable_use_card()->add_required_fields();
  *required_field->mutable_value_expression() =
      test_util::ValueExpressionBuilder()
          .addChunk(autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH)
          .addChunk(" - ")
          .addChunk(autofill::ServerFieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR)
          .toProto();
  *required_field->mutable_element() = ToSelectorProto("#expiration_date");

  Selector expiration_date_selector({"#expiration_date"});

  // Autofill succeeds.
  EXPECT_CALL(mock_web_controller_,
              FillCardForm(NotNull(), _, std::u16string(kFakeCvc16),
                           EqualsElement(test_util::MockFindElement(
                               mock_action_delegate_, fake_selector_)),
                           _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  // Validation fails when getting expiration date.
  EXPECT_CALL(
      mock_web_controller_,
      GetFieldValue(EqualsElement(test_util::MockFindElement(
                        mock_web_controller_, expiration_date_selector)),
                    _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));

  // Fallback succeeds.
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute("09 - 2050",
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, expiration_date_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Second validation succeeds.
  EXPECT_CALL(
      mock_web_controller_,
      GetFieldValue(EqualsElement(test_util::MockFindElement(
                        mock_web_controller_, expiration_date_selector)),
                    _))
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED,
            ProcessAction(action_proto));
}

TEST_F(UseCreditCardActionTest, FallbackFails) {
  ON_CALL(mock_web_controller_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action_proto = CreateUseCreditCardAction();
  AddRequiredField(
      &action_proto,
      static_cast<int>(
          autofill::ServerFieldType::CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR),
      "#expiration_date");

  Selector expiration_date_selector({"#expiration_date"});

  // Autofill succeeds.
  EXPECT_CALL(mock_web_controller_,
              FillCardForm(NotNull(), _, std::u16string(kFakeCvc16),
                           EqualsElement(test_util::MockFindElement(
                               mock_action_delegate_, fake_selector_)),
                           _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  // Validation fails when getting expiration date.
  EXPECT_CALL(
      mock_web_controller_,
      GetFieldValue(EqualsElement(test_util::MockFindElement(
                        mock_web_controller_, expiration_date_selector)),
                    _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));

  // Fallback fails.
  EXPECT_CALL(
      mock_web_controller_,
      SetValueAttribute("09/2050",
                        EqualsElement(test_util::MockFindElement(
                            mock_action_delegate_, expiration_date_selector)),
                        _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(OTHER_ACTION_STATUS)));

  ProcessedActionProto processed_action;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&processed_action));

  UseCreditCardAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(processed_action.status(),
            ProcessedActionStatusProto::AUTOFILL_INCOMPLETE);
  EXPECT_TRUE(processed_action.has_status_details());
  EXPECT_EQ(processed_action.status_details()
                .autofill_error_info()
                .autofill_field_error_size(),
            1);
  EXPECT_EQ(OTHER_ACTION_STATUS, processed_action.status_details()
                                     .autofill_error_info()
                                     .autofill_field_error(0)
                                     .status());
}

}  // namespace
}  // namespace autofill_assistant
