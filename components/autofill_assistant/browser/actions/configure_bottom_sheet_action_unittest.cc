// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/configure_bottom_sheet_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;

class ConfigureBottomSheetActionTest : public testing::Test {
 public:
  ConfigureBottomSheetActionTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetViewportMode())
        .WillByDefault(Invoke([this]() { return viewport_mode_; }));
    ON_CALL(mock_action_delegate_, SetViewportMode(_))
        .WillByDefault(
            Invoke([this](ViewportMode value) { viewport_mode_ = value; }));
    ON_CALL(mock_action_delegate_, GetPeekMode())
        .WillByDefault(Invoke([this]() { return peek_mode_; }));
    ON_CALL(mock_action_delegate_, SetPeekMode(_))
        .WillByDefault(
            Invoke([this](ConfigureBottomSheetProto::PeekMode peek_mode) {
              peek_mode_ = peek_mode;
            }));
    ON_CALL(mock_action_delegate_, OnWaitForWindowHeightChange(_))
        .WillByDefault(Invoke(
            [this](base::OnceCallback<void(const ClientStatus&)>& callback) {
              on_resize_cb_ = std::move(callback);
            }));
  }

 protected:
  // Runs the action defined in |proto_| and reports the result to |callback_|.
  //
  // Once it has run, the result of the action is available in
  // |processed_action_|. Before the action has run, |processed_action_| status
  // is UNKNOWN_ACTION_STATUS.
  void Run() {
    ActionProto action_proto;
    *action_proto.mutable_configure_bottom_sheet() = proto_;
    action_ = std::make_unique<ConfigureBottomSheetAction>(
        &mock_action_delegate_, action_proto);
    action_->ProcessAction(
        base::BindOnce(base::BindLambdaForTesting(
            [&](std::unique_ptr<ProcessedActionProto> result) {
              processed_action_ = *result;
            })));
  }

  // Runs an action that waits for a resize.
  void RunWithTimeout() {
    proto_.set_resize_timeout_ms(100);
    Run();
  }

  // Fast forward time enough for an action created by RunWithTimeout() to time
  // out.
  void ForceTimeout() {
    task_env_.FastForwardBy(base::TimeDelta::FromMilliseconds(100));
    task_env_.FastForwardBy(base::TimeDelta::FromMilliseconds(100));
  }

  // task_env_ must be first to guarantee other field
  // creation run in that environment.
  base::test::TaskEnvironment task_env_;

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  ConfigureBottomSheetProto proto_;
  ViewportMode viewport_mode_ = ViewportMode::NO_RESIZE;
  base::OnceCallback<void(const ClientStatus&)> on_resize_cb_;
  ConfigureBottomSheetProto::PeekMode peek_mode_ =
      ConfigureBottomSheetProto::HANDLE;
  std::unique_ptr<ConfigureBottomSheetAction> action_;
  ProcessedActionProto processed_action_;
};

TEST_F(ConfigureBottomSheetActionTest, NoOp) {
  Run();

  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
  EXPECT_EQ(ViewportMode::NO_RESIZE, viewport_mode_);
  EXPECT_EQ(ConfigureBottomSheetProto::HANDLE, peek_mode_);
}

TEST_F(ConfigureBottomSheetActionTest, ChangePeekMode) {
  proto_.set_peek_mode(ConfigureBottomSheetProto::HANDLE_HEADER);
  Run();

  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
  EXPECT_EQ(ViewportMode::NO_RESIZE, viewport_mode_);
  EXPECT_EQ(ConfigureBottomSheetProto::HANDLE_HEADER, peek_mode_);
}

TEST_F(ConfigureBottomSheetActionTest, EnableResize) {
  proto_.set_viewport_resizing(
      ConfigureBottomSheetProto::RESIZE_LAYOUT_VIEWPORT);
  Run();

  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
  EXPECT_EQ(ViewportMode::RESIZE_LAYOUT_VIEWPORT, viewport_mode_);
  EXPECT_EQ(ConfigureBottomSheetProto::HANDLE, peek_mode_);
}

TEST_F(ConfigureBottomSheetActionTest, EnableVisualViewportResize) {
  proto_.set_viewport_resizing(
      ConfigureBottomSheetProto::RESIZE_VISUAL_VIEWPORT);
  Run();

  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
  EXPECT_EQ(ViewportMode::RESIZE_VISUAL_VIEWPORT, viewport_mode_);
  EXPECT_EQ(ConfigureBottomSheetProto::HANDLE, peek_mode_);
}

TEST_F(ConfigureBottomSheetActionTest, EnableResizeWithPeekMode) {
  proto_.set_viewport_resizing(
      ConfigureBottomSheetProto::RESIZE_LAYOUT_VIEWPORT);
  proto_.set_peek_mode(ConfigureBottomSheetProto::HANDLE_HEADER);
  Run();

  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
  EXPECT_EQ(ViewportMode::RESIZE_LAYOUT_VIEWPORT, viewport_mode_);
  EXPECT_EQ(ConfigureBottomSheetProto::HANDLE_HEADER, peek_mode_);
}

TEST_F(ConfigureBottomSheetActionTest, WaitAfterSettingResize) {
  proto_.set_viewport_resizing(
      ConfigureBottomSheetProto::RESIZE_LAYOUT_VIEWPORT);

  RunWithTimeout();

  EXPECT_EQ(ViewportMode::RESIZE_LAYOUT_VIEWPORT, viewport_mode_);
  ASSERT_TRUE(on_resize_cb_);

  std::move(on_resize_cb_).Run(OkClientStatus());
  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
}

TEST_F(ConfigureBottomSheetActionTest, WaitFailsAfterSettingResize) {
  proto_.set_viewport_resizing(
      ConfigureBottomSheetProto::RESIZE_LAYOUT_VIEWPORT);

  RunWithTimeout();

  ASSERT_TRUE(on_resize_cb_);

  std::move(on_resize_cb_).Run(ClientStatus(OTHER_ACTION_STATUS));

  EXPECT_EQ(OTHER_ACTION_STATUS, processed_action_.status());
}

TEST_F(ConfigureBottomSheetActionTest, WaitTimesOut) {
  proto_.set_viewport_resizing(
      ConfigureBottomSheetProto::RESIZE_LAYOUT_VIEWPORT);

  RunWithTimeout();

  ASSERT_TRUE(on_resize_cb_);

  ForceTimeout();

  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
  EXPECT_EQ(TIMED_OUT, processed_action_.status_details().original_status());
}

TEST_F(ConfigureBottomSheetActionTest, TimesOutAfterWindowResized) {
  proto_.set_viewport_resizing(
      ConfigureBottomSheetProto::RESIZE_LAYOUT_VIEWPORT);

  RunWithTimeout();

  ASSERT_TRUE(on_resize_cb_);

  std::move(on_resize_cb_).Run(OkClientStatus());
  ForceTimeout();

  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
  EXPECT_EQ(ViewportMode::RESIZE_LAYOUT_VIEWPORT, viewport_mode_);
}

TEST_F(ConfigureBottomSheetActionTest, WindowResizedAfterTimeout) {
  proto_.set_viewport_resizing(
      ConfigureBottomSheetProto::RESIZE_LAYOUT_VIEWPORT);

  RunWithTimeout();

  ASSERT_TRUE(on_resize_cb_);

  ForceTimeout();
  std::move(on_resize_cb_).Run(OkClientStatus());
  EXPECT_EQ(ACTION_APPLIED, processed_action_.status());
}

TEST_F(ConfigureBottomSheetActionTest, WaitAfterUnsettingResize) {
  viewport_mode_ = ViewportMode::RESIZE_LAYOUT_VIEWPORT;
  proto_.set_viewport_resizing(ConfigureBottomSheetProto::NO_RESIZE);

  RunWithTimeout();

  ASSERT_TRUE(on_resize_cb_);
}

TEST_F(ConfigureBottomSheetActionTest, WaitAfterChangingPeekModeInResizeMode) {
  viewport_mode_ = ViewportMode::RESIZE_LAYOUT_VIEWPORT;
  proto_.set_peek_mode(ConfigureBottomSheetProto::HANDLE_HEADER);

  RunWithTimeout();

  EXPECT_EQ(ConfigureBottomSheetProto::HANDLE_HEADER, peek_mode_);
  ASSERT_TRUE(on_resize_cb_);
}

TEST_F(ConfigureBottomSheetActionTest, DontWaitAfterChangingPeekIfNoResize) {
  proto_.set_peek_mode(ConfigureBottomSheetProto::HANDLE_HEADER);

  RunWithTimeout();

  ASSERT_FALSE(on_resize_cb_);
}

TEST_F(ConfigureBottomSheetActionTest, DontWaitIfPeekModeNotChanged) {
  viewport_mode_ = ViewportMode::RESIZE_LAYOUT_VIEWPORT;
  proto_.set_peek_mode(ConfigureBottomSheetProto::HANDLE);

  RunWithTimeout();

  ASSERT_FALSE(on_resize_cb_);
}

TEST_F(ConfigureBottomSheetActionTest, DontWaitIfResizeModeNotChanged) {
  viewport_mode_ = ViewportMode::RESIZE_LAYOUT_VIEWPORT;
  proto_.set_viewport_resizing(
      ConfigureBottomSheetProto::RESIZE_LAYOUT_VIEWPORT);

  RunWithTimeout();

  ASSERT_FALSE(on_resize_cb_);
}

TEST_F(ConfigureBottomSheetActionTest, DontWaitIfResizeVisualViewport) {
  proto_.set_viewport_resizing(
      ConfigureBottomSheetProto::RESIZE_VISUAL_VIEWPORT);

  RunWithTimeout();

  ASSERT_FALSE(on_resize_cb_);
}

TEST_F(ConfigureBottomSheetActionTest,
       DontWaitIfDisablingResizeVisualViewport) {
  viewport_mode_ = ViewportMode::RESIZE_VISUAL_VIEWPORT;
  proto_.set_viewport_resizing(ConfigureBottomSheetProto::NO_RESIZE);

  RunWithTimeout();

  ASSERT_FALSE(on_resize_cb_);
}

TEST_F(ConfigureBottomSheetActionTest, WaitIfResizeVisualAfterLayoutViewport) {
  viewport_mode_ = ViewportMode::RESIZE_LAYOUT_VIEWPORT;
  proto_.set_viewport_resizing(
      ConfigureBottomSheetProto::RESIZE_VISUAL_VIEWPORT);

  RunWithTimeout();

  ASSERT_TRUE(on_resize_cb_);
}

TEST_F(ConfigureBottomSheetActionTest, WaitIfResizeLayoutAfterVisualViewport) {
  viewport_mode_ = ViewportMode::RESIZE_VISUAL_VIEWPORT;
  proto_.set_viewport_resizing(
      ConfigureBottomSheetProto::RESIZE_LAYOUT_VIEWPORT);

  RunWithTimeout();

  ASSERT_TRUE(on_resize_cb_);
}

}  // namespace
}  // namespace autofill_assistant
