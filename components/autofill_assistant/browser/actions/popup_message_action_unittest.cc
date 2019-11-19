// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/popup_message_action.h"

#include <string>
#include <utility>

#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::InSequence;
using ::testing::Pointee;
using ::testing::Property;

class PopupMessageActionTest : public testing::Test {
 public:
  void SetUp() override { prompt_proto_ = proto_.mutable_popup_message(); }

 protected:
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ActionProto proto_;
  PopupMessageProto* prompt_proto_;
};

TEST_F(PopupMessageActionTest, NoMessage) {
  {
    InSequence seq;

    EXPECT_CALL(mock_action_delegate_, SetBubbleMessage(""));
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  }
  PopupMessageAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
}

TEST_F(PopupMessageActionTest, WithMessage) {
  const std::string message = "test bubble message";
  {
    InSequence seq;

    EXPECT_CALL(mock_action_delegate_, SetBubbleMessage(message));
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  }
  prompt_proto_->set_message(message);
  PopupMessageAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant
