// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/tell_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrEq;

class TellActionTest : public testing::Test {
 public:
  TellActionTest() {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, SetStatusMessage(_)).WillByDefault(Return());
  }

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_tell() = proto_;
    TellAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  TellProto proto_;
};

TEST_F(TellActionTest, EmptyProtoSetsMessageDoesNothing) {
  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(_)).Times(0);
  // The needs_ui default is true.
  EXPECT_CALL(mock_action_delegate_, RequireUI());
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(TellActionTest, EmptyStringSetsMessage) {
  proto_.set_message("");

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(StrEq("")));
  // The needs_ui default is true.
  EXPECT_CALL(mock_action_delegate_, RequireUI());
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(TellActionTest, SetStatusMessageCalled) {
  proto_.set_message(" test_string ");

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(StrEq(" test_string ")));
  EXPECT_CALL(mock_action_delegate_, RequireUI());
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(TellActionTest, RequireUI) {
  proto_.set_message(" test_string ");
  proto_.set_needs_ui(false);

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(StrEq(" test_string ")));
  EXPECT_CALL(mock_action_delegate_, RequireUI()).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(TellActionTest, SetTtsMessage) {
  proto_.set_message("status_message");
  proto_.mutable_text_to_speech()->set_tts_message("tts_message");

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(StrEq("status_message")));
  EXPECT_CALL(mock_action_delegate_, SetTtsMessage(StrEq("tts_message")));
  EXPECT_CALL(mock_action_delegate_, MaybePlayTtsMessage()).Times(0);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(TellActionTest, PlayNowWithDefaultButtonStateStartsTts) {
  proto_.set_message("status_message");
  proto_.mutable_text_to_speech()->set_play_now(true);
  ON_CALL(mock_action_delegate_, GetTtsButtonState())
      .WillByDefault(Return(TtsButtonState::DEFAULT));

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(StrEq("status_message")));
  EXPECT_CALL(mock_action_delegate_, MaybePlayTtsMessage());

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(TellActionTest, PlayNowWithPlayingButtonStateReplacesTts) {
  proto_.set_message("status_message");
  proto_.mutable_text_to_speech()->set_play_now(true);
  ON_CALL(mock_action_delegate_, GetTtsButtonState())
      .WillByDefault(Return(TtsButtonState::PLAYING));

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(StrEq("status_message")));
  EXPECT_CALL(mock_action_delegate_, MaybePlayTtsMessage());

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(TellActionTest, PlayNowWithDisabledButtonStateDoesNotStartTts) {
  proto_.set_message("status_message");
  proto_.mutable_text_to_speech()->set_play_now(true);
  ON_CALL(mock_action_delegate_, GetTtsButtonState())
      .WillByDefault(Return(TtsButtonState::DISABLED));

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(StrEq("status_message")));
  EXPECT_CALL(mock_action_delegate_, MaybePlayTtsMessage()).Times(0);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
