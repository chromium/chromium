// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_progress_bar_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/view_layout.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrEq;

class ShowProgressBarActionTest : public testing::Test {
 public:
  ShowProgressBarActionTest() {}

  void SetUp() override {}

 protected:
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_show_progress_bar() = proto_;
    ShowProgressBarAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ShowProgressBarProto proto_;
};

TEST_F(ShowProgressBarActionTest, EmptyProgressBarDoesNothing) {
  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(_)).Times(0);
  EXPECT_CALL(mock_action_delegate_, SetProgressVisible(_)).Times(0);
  EXPECT_CALL(mock_action_delegate_, SetStepProgressBarConfiguration(_))
      .Times(0);
  EXPECT_CALL(mock_action_delegate_, SetProgress(_)).Times(0);
  EXPECT_CALL(mock_action_delegate_, SetProgressActiveStep(_)).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowProgressBarActionTest, SpecifiedMessageGetsSet) {
  proto_.set_message("Message");

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage("Message"));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowProgressBarActionTest, HidesProgressBar) {
  proto_.set_hide(true);

  EXPECT_CALL(mock_action_delegate_, SetProgressVisible(false));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowProgressBarActionTest, ShowsProgressBar) {
  proto_.set_hide(false);

  EXPECT_CALL(mock_action_delegate_, SetProgressVisible(true));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowProgressBarActionTest, FewerThanTwoStepsProgressBarFailsAction) {
  auto* config = proto_.mutable_step_progress_bar_configuration();
  config->set_use_step_progress_bar(true);
  config->add_annotated_step_icons()->mutable_icon()->set_icon(
      DrawableProto::PROGRESSBAR_DEFAULT_INITIAL_STEP);

  EXPECT_CALL(mock_action_delegate_, SetStepProgressBarConfiguration(_))
      .Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(ShowProgressBarActionTest, UpdateStepProgressBarConfiguration) {
  auto* config = proto_.mutable_step_progress_bar_configuration();
  config->set_use_step_progress_bar(true);
  config->add_annotated_step_icons()->mutable_icon()->set_icon(
      DrawableProto::PROGRESSBAR_DEFAULT_INITIAL_STEP);
  config->add_annotated_step_icons()->mutable_icon()->set_icon(
      DrawableProto::PROGRESSBAR_DEFAULT_FINAL_STEP);

  EXPECT_CALL(mock_action_delegate_, SetStepProgressBarConfiguration(_));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowProgressBarActionTest, DeactivateStepProgressBar) {
  auto* config = proto_.mutable_step_progress_bar_configuration();
  config->set_use_step_progress_bar(false);

  EXPECT_CALL(mock_action_delegate_, SetStepProgressBarConfiguration(_));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowProgressBarActionTest, SetProgress) {
  proto_.set_progress(50);

  EXPECT_CALL(mock_action_delegate_, SetProgress(50));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowProgressBarActionTest, ClampsProgress) {
  proto_.set_progress(150);

  EXPECT_CALL(mock_action_delegate_, SetProgress(100));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowProgressBarActionTest, SetActiveStep) {
  proto_.set_active_step(2);

  EXPECT_CALL(mock_action_delegate_, SetProgressActiveStep(2));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowProgressBarActionTest, SetActiveStepFromIdentifier) {
  proto_.set_active_step_identifier("id");

  EXPECT_CALL(mock_action_delegate_, SetProgressActiveStepIdentifier("id"))
      .WillOnce(Return(true));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowProgressBarActionTest, SetActiveStepFromUnknownIdentifier) {
  proto_.set_active_step_identifier("unknown id");

  EXPECT_CALL(mock_action_delegate_,
              SetProgressActiveStepIdentifier("unknown id"))
      .WillOnce(Return(false));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  Run();
}

TEST_F(ShowProgressBarActionTest, SetProgressToComplete) {
  proto_.set_complete_progress(true);

  EXPECT_CALL(mock_action_delegate_, SetProgress(100));
  EXPECT_CALL(mock_action_delegate_, SetProgressActiveStep(-1));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

TEST_F(ShowProgressBarActionTest, SetErrorState) {
  proto_.set_error_state(true);

  EXPECT_CALL(mock_action_delegate_, SetProgressBarErrorState(true));
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  Run();
}

}  // namespace
}  // namespace autofill_assistant
