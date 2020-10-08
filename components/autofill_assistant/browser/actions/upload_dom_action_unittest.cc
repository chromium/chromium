// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/upload_dom_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/action_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/selector.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::AllOf;
using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Property;

class UploadDomActionTest : public testing::Test {
 public:
  UploadDomActionTest() {}

  void SetUp() override {}

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_upload_dom() = proto_;
    UploadDomAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  UploadDomProto proto_;
};

TEST_F(UploadDomActionTest, EmptySelectorFails) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_SELECTOR))));
  Run();
}

TEST_F(UploadDomActionTest, ActionFailsForNonExistentElement) {
  InSequence sequence;

  Selector selector({"#element"});
  *proto_.mutable_tree_root() = selector.proto;

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(selector, _))
      .WillOnce(RunOnceCallback<1>(ClientStatus(TIMED_OUT)));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(Property(&ProcessedActionProto::status, TIMED_OUT),
                        Property(&ProcessedActionProto::html_source, "")))));
  Run();
}

TEST_F(UploadDomActionTest, CheckExpectedCallChain) {
  InSequence sequence;

  Selector selector({"#element"});
  *proto_.mutable_tree_root() = selector.proto;

  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  auto expected_element =
      test_util::MockFindElement(mock_action_delegate_, selector);
  EXPECT_CALL(mock_action_delegate_,
              GetOuterHtml(EqualsElement(expected_element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "<html></html>"));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::html_source, "<html></html>")))));
  Run();
}

TEST_F(UploadDomActionTest, ReturnsEmptyStringForNotFoundElement) {
  InSequence sequence;

  Selector selector({"#element"});
  *proto_.mutable_tree_root() = selector.proto;

  // Unlikely scenario, if ShortWaitForElement succeeds, the FindElement will
  // too. Failing FindElement is however the more interesting test than failing
  // GetOuterHtml.
  EXPECT_CALL(mock_action_delegate_, OnShortWaitForElement(selector, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus()));
  EXPECT_CALL(mock_action_delegate_, FindElement(selector, _))
      .WillOnce(
          RunOnceCallback<1>(ClientStatus(ELEMENT_RESOLUTION_FAILED), nullptr));
  EXPECT_CALL(mock_action_delegate_, GetOuterHtml(_, _)).Times(0);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ELEMENT_RESOLUTION_FAILED),
          Property(&ProcessedActionProto::html_source, "")))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
