// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/check_option_element_action.h"

#include "base/guid.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/dom_action.pb.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/web/element_finder_result.h"
#include "components/autofill_assistant/browser/web/element_store.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

const char kSelectId[] = "select";
const char kOptionId[] = "option";

class CheckOptionElementActionTest : public testing::Test {
 public:
  CheckOptionElementActionTest() {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));

    proto_.mutable_select_id()->set_identifier(kSelectId);
    proto_.mutable_option_id()->set_identifier(kOptionId);
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_check_option_element() = proto_;
    CheckOptionElementAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  CheckOptionElementProto proto_;
};

TEST_F(CheckOptionElementActionTest, UnknownSelectElementFails) {
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              CLIENT_ID_RESOLUTION_FAILED))));
  Run();
}

TEST_F(CheckOptionElementActionTest, UnknownOptionElementFails) {
  ElementFinderResult select;
  mock_action_delegate_.GetElementStore()->AddElement(kSelectId,
                                                      select.dom_object());

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              CLIENT_ID_RESOLUTION_FAILED))));
  Run();
}

TEST_F(CheckOptionElementActionTest, SucceedsForMatchingOption) {
  ElementFinderResult select;
  select.SetObjectId("select");
  mock_action_delegate_.GetElementStore()->AddElement(kSelectId,
                                                      select.dom_object());
  ElementFinderResult option;
  option.SetObjectId("option");
  mock_action_delegate_.GetElementStore()->AddElement(kOptionId,
                                                      option.dom_object());

  EXPECT_CALL(mock_web_controller_,
              CheckSelectedOptionElement(EqualsElement(option),
                                         EqualsElement(select), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  EXPECT_CALL(
      callback_,
      Run(Pointee(
          AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                Property(&ProcessedActionProto::check_option_element_result,
                         AllOf(Property(&CheckOptionElementProto::Result::match,
                                        true)))))));
  Run();
}

TEST_F(CheckOptionElementActionTest, DoesNotFailForMismatch) {
  ElementFinderResult select;
  select.SetObjectId("select");
  mock_action_delegate_.GetElementStore()->AddElement(kSelectId,
                                                      select.dom_object());
  ElementFinderResult option;
  option.SetObjectId("option");
  mock_action_delegate_.GetElementStore()->AddElement(kOptionId,
                                                      option.dom_object());

  EXPECT_CALL(mock_web_controller_,
              CheckSelectedOptionElement(EqualsElement(option),
                                         EqualsElement(select), _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(ELEMENT_MISMATCH)));

  proto_.set_mismatch_should_fail(false);

  EXPECT_CALL(
      callback_,
      Run(Pointee(
          AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                Property(&ProcessedActionProto::check_option_element_result,
                         AllOf(Property(&CheckOptionElementProto::Result::match,
                                        false)))))));
  Run();
}

TEST_F(CheckOptionElementActionTest, FailsForMismatchIfSpecified) {
  ElementFinderResult select;
  select.SetObjectId("select");
  mock_action_delegate_.GetElementStore()->AddElement(kSelectId,
                                                      select.dom_object());
  ElementFinderResult option;
  option.SetObjectId("option");
  mock_action_delegate_.GetElementStore()->AddElement(kOptionId,
                                                      option.dom_object());

  EXPECT_CALL(mock_web_controller_,
              CheckSelectedOptionElement(EqualsElement(option),
                                         EqualsElement(select), _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(ELEMENT_MISMATCH)));

  proto_.set_mismatch_should_fail(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(
          AllOf(Property(&ProcessedActionProto::status, ELEMENT_MISMATCH),
                Property(&ProcessedActionProto::check_option_element_result,
                         AllOf(Property(&CheckOptionElementProto::Result::match,
                                        false)))))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
