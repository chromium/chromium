// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_address_action.h"

#include <utility>

#include "base/guid.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {
const char kAddressName[] = "billing";
const char kFakeSelector[] = "#selector";
const char kFirstName[] = "FirstName";
const char kLastName[] = "LastName";
const char kEmail[] = "foobar@gmail.com";
const char kPhoneNumber[] = "+41791234567";
const char kModelIdentifier[] = "identifier";

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::SaveArgPointee;

class UseAddressActionTest : public testing::Test {
 public:
  void SetUp() override {
    autofill::test::SetProfileInfo(&profile_, kFirstName, "", kLastName, kEmail,
                                   "", "", "", "", "", "", "", kPhoneNumber);
    // Store copies of |profile_| in |user_data_| and |user_model_|.
    user_data_.selected_addresses_[kAddressName] =
        std::make_unique<autofill::AutofillProfile>(profile_);
    auto profiles = std::make_unique<
        std::vector<std::unique_ptr<autofill::AutofillProfile>>>();
    profiles->emplace_back(
        std::make_unique<autofill::AutofillProfile>(profile_));
    user_model_.SetAutofillProfiles(std::move(profiles));
    ValueProto profile_value;
    profile_value.mutable_profiles()->add_values()->set_guid(profile_.guid());
    user_model_.SetValue(kModelIdentifier, profile_value);

    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, GetUserModel)
        .WillByDefault(Return(&user_model_));
    ON_CALL(mock_action_delegate_, GetPersonalDataManager)
        .WillByDefault(Return(&mock_personal_data_manager_));
    ON_CALL(mock_action_delegate_, RunElementChecks)
        .WillByDefault(Invoke([this](BatchElementChecker* checker) {
          checker->Run(&mock_web_controller_);
        }));
    ON_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
  }

 protected:
  ActionProto CreateUseAddressAction() {
    ActionProto action;
    UseAddressProto* use_address = action.mutable_use_address();
    use_address->set_name(kAddressName);
    *use_address->mutable_form_field_element() = ToSelectorProto(kFakeSelector);
    return action;
  }

  UseAddressProto::RequiredField* AddRequiredField(ActionProto* action,
                                                   std::string value_expression,
                                                   std::string selector) {
    auto* required_field = action->mutable_use_address()->add_required_fields();
    required_field->set_value_expression(value_expression);
    *required_field->mutable_element() = ToSelectorProto(selector);
    return required_field;
  }

  ProcessedActionStatusProto ProcessAction(const ActionProto& action_proto) {
    UseAddressAction action(&mock_action_delegate_, action_proto);
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
  autofill::AutofillProfile profile_ = {base::GenerateGUID(),
                                        autofill::test::kEmptyOrigin};
};

#if !defined(OS_ANDROID)
#define MAYBE_FillManually FillManually
#else
#define MAYBE_FillManually DISABLED_FillManually
#endif
TEST_F(UseAddressActionTest, MAYBE_FillManually) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();

  EXPECT_EQ(ProcessedActionStatusProto::MANUAL_FALLBACK,
            ProcessAction(action_proto));
}

TEST_F(UseAddressActionTest, InvalidActionNoSelectorSet) {
  ActionProto action;
  action.mutable_use_address();
  EXPECT_EQ(ProcessedActionStatusProto::INVALID_ACTION, ProcessAction(action));
}

TEST_F(UseAddressActionTest, InvalidActionNameSetButEmpty) {
  ActionProto action;
  UseAddressProto* use_address = action.mutable_use_address();
  *use_address->mutable_form_field_element() = ToSelectorProto(kFakeSelector);
  use_address->set_name("");
  EXPECT_EQ(ProcessedActionStatusProto::INVALID_ACTION, ProcessAction(action));
}

TEST_F(UseAddressActionTest, InvalidActionSkipAutofillWithoutRequiredFields) {
  ActionProto action;
  UseAddressProto* use_address = action.mutable_use_address();
  use_address->set_name(kAddressName);
  use_address->set_skip_autofill(true);
  EXPECT_EQ(ProcessedActionStatusProto::INVALID_ACTION, ProcessAction(action));
}

TEST_F(UseAddressActionTest, PreconditionFailedNoProfileForName) {
  ActionProto action;
  UseAddressProto* use_address = action.mutable_use_address();
  *use_address->mutable_form_field_element() = ToSelectorProto(kFakeSelector);
  use_address->set_name("invalid");
  EXPECT_EQ(ProcessedActionStatusProto::PRECONDITION_FAILED,
            ProcessAction(action));
}

TEST_F(UseAddressActionTest, ResolveProfileByNameSucceeds) {
  ON_CALL(mock_action_delegate_,
          OnShortWaitForElement(Selector({kFakeSelector}).MustBeVisible(), _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  ActionProto action;
  UseAddressProto* use_address = action.mutable_use_address();
  *use_address->mutable_form_field_element() = ToSelectorProto(kFakeSelector);
  use_address->set_name(kAddressName);
  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(Pointee(Eq(profile_)), _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(UseAddressActionTest, InvalidActionModelIdentifierSetButEmpty) {
  ActionProto action;
  UseAddressProto* use_address = action.mutable_use_address();
  *use_address->mutable_form_field_element() = ToSelectorProto(kFakeSelector);
  use_address->set_model_identifier("");
  EXPECT_EQ(ProcessedActionStatusProto::INVALID_ACTION, ProcessAction(action));
}

TEST_F(UseAddressActionTest, PreconditionFailedNoProfileForModelIdentifier) {
  ActionProto action;
  UseAddressProto* use_address = action.mutable_use_address();
  *use_address->mutable_form_field_element() = ToSelectorProto(kFakeSelector);
  use_address->set_model_identifier("invalid");
  EXPECT_EQ(ProcessedActionStatusProto::PRECONDITION_FAILED,
            ProcessAction(action));
}

TEST_F(UseAddressActionTest, ResolveProfileByModelIdentifierSucceeds) {
  ON_CALL(mock_action_delegate_,
          OnShortWaitForElement(Selector({kFakeSelector}).MustBeVisible(), _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  ActionProto action;
  UseAddressProto* use_address = action.mutable_use_address();
  *use_address->mutable_form_field_element() = ToSelectorProto(kFakeSelector);
  use_address->set_model_identifier(kModelIdentifier);
  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(Pointee(Eq(profile_)), _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(UseAddressActionTest, PreconditionFailedPopulatesUnexpectedErrorInfo) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  user_data_.selected_addresses_[kAddressName] = nullptr;
  user_data_.selected_addresses_["one_more"] = nullptr;

  UseAddressAction action(&mock_action_delegate_, action_proto);

  ProcessedActionProto processed_action;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&processed_action));
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(ProcessedActionStatusProto::PRECONDITION_FAILED,
            processed_action.status());
  const auto& error_info =
      processed_action.status_details().autofill_error_info();
  EXPECT_EQ(base::JoinString({kAddressName, "one_more"}, ","),
            error_info.client_memory_address_key_names());
  EXPECT_EQ(kAddressName, error_info.address_key_requested());
  EXPECT_TRUE(error_info.address_pointee_was_null());
}

TEST_F(UseAddressActionTest, ShortWaitForElementVisible) {
  EXPECT_CALL(
      mock_action_delegate_,
      OnShortWaitForElement(Selector({kFakeSelector}).MustBeVisible(), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));

  ActionProto action_proto = CreateUseAddressAction();
  // Autofill succeeds.
  EXPECT_CALL(mock_action_delegate_, OnFillAddressForm(NotNull(), _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Validation succeeds.
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED,
            ProcessAction(action_proto));
}

TEST_F(UseAddressActionTest, ValidationSucceeds) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  AddRequiredField(&action_proto,
                   base::StrCat({"${",
                                 base::NumberToString(static_cast<int>(
                                     autofill::ServerFieldType::NAME_FIRST)),
                                 "}"}),
                   "#first_name");
  AddRequiredField(&action_proto,
                   base::StrCat({"${",
                                 base::NumberToString(static_cast<int>(
                                     autofill::ServerFieldType::NAME_LAST)),
                                 "}"}),
                   "#last_name");
  AddRequiredField(&action_proto,
                   base::StrCat({"${",
                                 base::NumberToString(static_cast<int>(
                                     autofill::ServerFieldType::EMAIL_ADDRESS)),
                                 "}"}),
                   "#email");

  // Autofill succeeds.
  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(
                  NotNull(), Eq(Selector({kFakeSelector}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Validation succeeds.
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED,
            ProcessAction(action_proto));
}

TEST_F(UseAddressActionTest, FallbackFails) {
  ON_CALL(mock_action_delegate_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action_proto = CreateUseAddressAction();
  AddRequiredField(&action_proto,
                   base::StrCat({"${",
                                 base::NumberToString(static_cast<int>(
                                     autofill::ServerFieldType::NAME_FIRST)),
                                 "}"}),
                   "#first_name");
  AddRequiredField(&action_proto,
                   base::StrCat({"${",
                                 base::NumberToString(static_cast<int>(
                                     autofill::ServerFieldType::NAME_LAST)),
                                 "}"}),
                   "#last_name");
  AddRequiredField(&action_proto,
                   base::StrCat({"${",
                                 base::NumberToString(static_cast<int>(
                                     autofill::ServerFieldType::EMAIL_ADDRESS)),
                                 "}"}),
                   "#email");

  Selector email_selector({"#email"});
  Selector first_name_selector({"#first_name"});
  Selector last_name_selector({"#last_name"});

  // Autofill succeeds.
  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(
                  NotNull(), Eq(Selector({kFakeSelector}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Validation fails when getting FIRST_NAME.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(email_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(first_name_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(last_name_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  // Fallback fails.
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue(kFirstName,
                              EqualsElement(test_util::MockFindElement(
                                  mock_action_delegate_, first_name_selector)),
                              _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(OTHER_ACTION_STATUS)));

  ProcessedActionProto processed_action;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&processed_action));

  UseAddressAction action(&mock_action_delegate_, action_proto);
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

TEST_F(UseAddressActionTest, FallbackSucceeds) {
  ON_CALL(mock_action_delegate_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action_proto = CreateUseAddressAction();
  AddRequiredField(&action_proto,
                   base::StrCat({"${",
                                 base::NumberToString(static_cast<int>(
                                     autofill::ServerFieldType::NAME_FIRST)),
                                 "}"}),
                   "#first_name");
  AddRequiredField(&action_proto,
                   base::StrCat({"${",
                                 base::NumberToString(static_cast<int>(
                                     autofill::ServerFieldType::NAME_LAST)),
                                 "}"}),
                   "#last_name");
  AddRequiredField(&action_proto,
                   base::StrCat({"${",
                                 base::NumberToString(static_cast<int>(
                                     autofill::ServerFieldType::EMAIL_ADDRESS)),
                                 "}"}),
                   "#email");

  Selector email_selector({"#email"});
  Selector first_name_selector({"#first_name"});
  Selector last_name_selector({"#last_name"});

  // Autofill succeeds.
  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(
                  NotNull(), Eq(Selector({kFakeSelector}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Validation fails when getting FIRST_NAME.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(email_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(first_name_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(last_name_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  // Fallback succeeds.
  Expectation set_first_name =
      EXPECT_CALL(
          mock_action_delegate_,
          OnSetFieldValue(kFirstName,
                          EqualsElement(test_util::MockFindElement(
                              mock_action_delegate_, first_name_selector)),
                          _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .After(set_first_name)
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED,
            ProcessAction(action_proto));
}

TEST_F(UseAddressActionTest, AutofillFailureWithoutRequiredFieldsIsFatal) {
  ActionProto action_proto = CreateUseAddressAction();

  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(
                  NotNull(), Eq(Selector({kFakeSelector}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(OTHER_ACTION_STATUS)));

  ProcessedActionProto processed_action;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&processed_action));

  UseAddressAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(processed_action.status(),
            ProcessedActionStatusProto::OTHER_ACTION_STATUS);
  EXPECT_EQ(processed_action.has_status_details(), false);
}

TEST_F(UseAddressActionTest,
       AutofillFailureWithRequiredFieldsLaunchesFallback) {
  ON_CALL(mock_action_delegate_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action_proto = CreateUseAddressAction();
  AddRequiredField(&action_proto,
                   base::StrCat({"${",
                                 base::NumberToString(static_cast<int>(
                                     autofill::ServerFieldType::NAME_FIRST)),
                                 "}"}),
                   "#first_name");

  Selector first_name_selector({"#first_name"});

  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(
                  NotNull(), Eq(Selector({kFakeSelector}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<2>(
          FillAutofillErrorStatus(ClientStatus(OTHER_ACTION_STATUS))));

  // First validation fails.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(first_name_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  // Fill first name.
  Expectation set_first_name =
      EXPECT_CALL(
          mock_action_delegate_,
          OnSetFieldValue(kFirstName,
                          EqualsElement(test_util::MockFindElement(
                              mock_action_delegate_, first_name_selector)),
                          _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(first_name_selector, _))
      .After(set_first_name)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  ProcessedActionProto processed_action;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&processed_action));

  UseAddressAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(processed_action.status(),
            ProcessedActionStatusProto::ACTION_APPLIED);
  EXPECT_EQ(processed_action.status_details()
                .autofill_error_info()
                .autofill_error_status(),
            OTHER_ACTION_STATUS);
}

TEST_F(UseAddressActionTest, FallbackForPhoneSucceeds) {
  ON_CALL(mock_action_delegate_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action_proto = CreateUseAddressAction();
  AddRequiredField(&action_proto, "(+${12}) (${11}) ${10}", "#phone_number");

  Selector phone_number_selector({"#phone_number"});

  // Autofill succeeds.
  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(
                  NotNull(), Eq(Selector({kFakeSelector}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Validation fails when getting phone number.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(phone_number_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));

  // Fallback succeeds.
  Expectation set_phone_number_name =
      EXPECT_CALL(
          mock_action_delegate_,
          OnSetFieldValue("(+41) (79) 1234567",
                          EqualsElement(test_util::MockFindElement(
                              mock_action_delegate_, phone_number_selector)),
                          _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(phone_number_selector, _))
      .After(set_phone_number_name)
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED,
            ProcessAction(action_proto));
}

TEST_F(UseAddressActionTest, ForcedFallbackWithKeystrokes) {
  ON_CALL(mock_action_delegate_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action_proto = CreateUseAddressAction();
  auto* name_required = AddRequiredField(
      &action_proto,
      base::StrCat({"${",
                    base::NumberToString(static_cast<int>(
                        autofill::ServerFieldType::NAME_FIRST)),
                    "}"}),
      "#first_name");
  name_required->set_forced(true);
  name_required->set_fill_strategy(SIMULATE_KEY_PRESSES);
  name_required->set_delay_in_millisecond(1000);

  Selector first_name_selector({"#first_name"});

  // Autofill succeeds.
  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(
                  NotNull(), Eq(Selector({kFakeSelector}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // The field is not empty.
  ON_CALL(mock_web_controller_, OnGetFieldValue(first_name_selector, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  // But we still want the first name filled, with
  // simulated keypresses.
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue(kFirstName, true, 1000,
                              EqualsElement(test_util::MockFindElement(
                                  mock_action_delegate_, first_name_selector)),
                              _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED,
            ProcessAction(action_proto));
}

TEST_F(UseAddressActionTest, SkippingAutofill) {
  ON_CALL(mock_action_delegate_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action_proto;
  action_proto.mutable_use_address()->set_name(kAddressName);
  AddRequiredField(&action_proto,
                   base::StrCat({"${",
                                 base::NumberToString(static_cast<int>(
                                     autofill::ServerFieldType::NAME_FIRST)),
                                 "}"}),
                   "#first_name");
  action_proto.mutable_use_address()->set_skip_autofill(true);

  Selector first_name_selector({"#first_name"});

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(_, _)).Times(0);
  EXPECT_CALL(mock_action_delegate_, OnFillAddressForm(_, _, _)).Times(0);

  // First validation fails.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(first_name_selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  // Fill first name.
  Expectation set_first_name =
      EXPECT_CALL(
          mock_action_delegate_,
          OnSetFieldValue(kFirstName,
                          EqualsElement(test_util::MockFindElement(
                              mock_action_delegate_, first_name_selector)),
                          _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(first_name_selector, _))
      .After(set_first_name)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  ProcessedActionProto processed_action;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&processed_action));

  UseAddressAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(processed_action.status(),
            ProcessedActionStatusProto::ACTION_APPLIED);
}

}  // namespace
}  // namespace autofill_assistant
