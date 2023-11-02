// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/check_element_tag_action.h"

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

const char kClientId[] = "select";

class CheckElementTagActionTest : public testing::Test {
 public:
  CheckElementTagActionTest() {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetWebController)
        .WillByDefault(Return(&mock_web_controller_));

    proto_.mutable_client_id()->set_identifier(kClientId);
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_check_element_tag() = proto_;
    CheckElementTagAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  CheckElementTagProto proto_;
};

TEST_F(CheckElementTagActionTest, UnknownElementFails) {
  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              CLIENT_ID_RESOLUTION_FAILED))));
  Run();
}

TEST_F(CheckElementTagActionTest, SucceedsForMatchingTagCaseInsensitive) {
  ElementFinderResult element;
  element.SetObjectId("e");
  mock_action_delegate_.GetElementStore()->AddElement(kClientId,
                                                      element.dom_object());

  EXPECT_CALL(mock_web_controller_, GetElementTag(EqualsElement(element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "TAG"));

  proto_.add_any_of_tag("other");
  proto_.add_any_of_tag("tag");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(CheckElementTagActionTest, FailsForNoMatchingTags) {
  ElementFinderResult element;
  element.SetObjectId("e");
  mock_action_delegate_.GetElementStore()->AddElement(kClientId,
                                                      element.dom_object());

  EXPECT_CALL(mock_web_controller_, GetElementTag(EqualsElement(element), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "TAG"));

  proto_.add_any_of_tag("DIV");
  proto_.add_any_of_tag("P");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ELEMENT_MISMATCH))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
