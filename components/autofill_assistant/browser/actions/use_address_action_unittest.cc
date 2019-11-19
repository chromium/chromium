// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_address_action.h"

#include <utility>

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArgPointee;

class UseAddressActionTest : public testing::Test {
 public:
  void SetUp() override {
    // Build two identical autofill profiles. One for the memory, one for the
    // mock.
    auto autofill_profile = std::make_unique<autofill::AutofillProfile>(
        base::GenerateGUID(), autofill::test::kEmptyOrigin);
    autofill::test::SetProfileInfo(autofill_profile.get(), kFirstName, "",
                                   kLastName, kEmail, "", "", "", "", "", "",
                                   "", "");
    autofill::test::SetProfileInfo(&autofill_profile_, kFirstName, "",
                                   kLastName, kEmail, "", "", "", "", "", "",
                                   "", "");
    client_memory_.set_selected_address(kAddressName,
                                        std::move(autofill_profile));

    ON_CALL(mock_personal_data_manager_, GetProfileByGUID)
        .WillByDefault(Return(&autofill_profile_));
    ON_CALL(mock_action_delegate_, GetClientMemory)
        .WillByDefault(Return(&client_memory_));
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
  const char* const kAddressName = "billing";
  const char* const kFakeSelector = "#selector";
  const char* const kSelectionPrompt = "prompt";
  const char* const kFirstName = "FirstName";
  const char* const kLastName = "LastName";
  const char* const kEmail = "foobar@gmail.com";

  ActionProto CreateUseAddressAction() {
    ActionProto action;
    UseAddressProto* use_address = action.mutable_use_address();
    use_address->set_name(kAddressName);
    use_address->mutable_form_field_element()->add_selectors(kFakeSelector);
    return action;
  }

  UseAddressProto::RequiredField* AddRequiredField(
      ActionProto* action,
      UseAddressProto::RequiredField::AddressField type,
      std::string selector) {
    auto* required_field = action->mutable_use_address()->add_required_fields();
    required_field->set_address_field(type);
    required_field->mutable_element()->add_selectors(selector);
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
  ClientMemory client_memory_;

  autofill::AutofillProfile autofill_profile_;
};

#if !defined(OS_ANDROID)
#define MAYBE_FillManually FillManually
#else
#define MAYBE_FillManually DISABLED_FillManually
#endif
TEST_F(UseAddressActionTest, MAYBE_FillManually) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  action_proto.mutable_use_address()->set_prompt(kSelectionPrompt);

  EXPECT_EQ(ProcessedActionStatusProto::MANUAL_FALLBACK,
            ProcessAction(action_proto));
}

TEST_F(UseAddressActionTest, NoSelectedAddress) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  action_proto.mutable_use_address()->set_prompt(kSelectionPrompt);

  client_memory_.set_selected_address(kAddressName, nullptr);

  EXPECT_EQ(ProcessedActionStatusProto::PRECONDITION_FAILED,
            ProcessAction(action_proto));
}

TEST_F(UseAddressActionTest, PreconditionFailedPopulatesUnexpectedErrorInfo) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  action_proto.mutable_use_address()->set_prompt(kSelectionPrompt);
  client_memory_.set_selected_address(kAddressName, nullptr);
  client_memory_.set_selected_address("one_more", nullptr);

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
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::FIRST_NAME,
                   "#first_name");
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::LAST_NAME,
                   "#last_name");
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::EMAIL,
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
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::FIRST_NAME,
                   "#first_name");
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::LAST_NAME,
                   "#last_name");
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::EMAIL,
                   "#email");

  // Autofill succeeds.
  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(
                  NotNull(), Eq(Selector({kFakeSelector}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Validation fails when getting FIRST_NAME.
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(Eq(Selector({"#email"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(Eq(Selector({"#first_name"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(Eq(Selector({"#last_name"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  // Fallback fails.
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue(Eq(Selector({"#first_name"})), kFirstName, _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(OTHER_ACTION_STATUS)));

  EXPECT_EQ(ProcessedActionStatusProto::MANUAL_FALLBACK,
            ProcessAction(action_proto));
}

TEST_F(UseAddressActionTest, FallbackSucceeds) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::FIRST_NAME,
                   "#first_name");
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::LAST_NAME,
                   "#last_name");
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::EMAIL,
                   "#email");

  // Autofill succeeds.
  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(
                  NotNull(), Eq(Selector({kFakeSelector}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  {
    InSequence seq;

    // Validation fails when getting FIRST_NAME.
    EXPECT_CALL(mock_web_controller_,
                OnGetFieldValue(Eq(Selector({"#email"})), _))
        .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));
    EXPECT_CALL(mock_web_controller_,
                OnGetFieldValue(Eq(Selector({"#first_name"})), _))
        .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
    EXPECT_CALL(mock_web_controller_,
                OnGetFieldValue(Eq(Selector({"#last_name"})), _))
        .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

    // Fallback succeeds.
    EXPECT_CALL(mock_action_delegate_,
                OnSetFieldValue(Eq(Selector({"#first_name"})), kFirstName, _))
        .WillOnce(RunOnceCallback<2>(OkClientStatus()));

    // Second validation succeeds.
    EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
        .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), "not empty"));
  }
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
  ActionProto action_proto = CreateUseAddressAction();
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::FIRST_NAME,
                   "#first_name");

  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(
                  NotNull(), Eq(Selector({kFakeSelector}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<2>(
          FillAutofillErrorStatus(ClientStatus(OTHER_ACTION_STATUS))));

  // First validation fails.
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(Selector({"#first_name"}), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  // Fill first name.
  Expectation set_first_name =
      EXPECT_CALL(mock_action_delegate_,
                  OnSetFieldValue(Selector({"#first_name"}), kFirstName, _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(Selector({"#first_name"}), _))
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

}  // namespace
}  // namespace autofill_assistant
