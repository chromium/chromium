// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_run_progress.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_animated_icon.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"

using autofill_assistant::password_change::ProgressStep;
using ChildViewId = PasswordChangeRunProgress::ChildViewId;
using ::testing::StrictMock;

class PasswordChangeRunProgressTest : public views::ViewsTestBase {
 public:
  PasswordChangeRunProgressTest() = default;
  ~PasswordChangeRunProgressTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    password_change_run_progress_ = widget_->SetContentsView(
        std::make_unique<PasswordChangeRunProgress>(base::BindRepeating(
            &PasswordChangeRunProgressTest::OnChildAnimationContainerSet,
            base::Unretained(this))));
  }
  PasswordChangeRunProgress* run_progress() {
    return password_change_run_progress_;
  }

  void TearDown() override {
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  void OnChildAnimationContainerSet(ChildViewId id,
                                    gfx::AnimationContainer* container) {
    test_apis_[id].reset();
    if (container) {
      test_apis_[id] =
          std::make_unique<gfx::AnimationContainerTestApi>(container);
    }
  }

  void AdvanceTime(base::TimeDelta time) {
    for (auto& [id, api] : test_apis_) {
      if (api) {
        api->IncrementTime(time);
      }
    }
  }

 private:
  raw_ptr<PasswordChangeRunProgress> password_change_run_progress_ = nullptr;

  // Widget to anchor the view and retrieve a color provider from.
  std::unique_ptr<views::Widget> widget_;
  base::flat_map<ChildViewId, std::unique_ptr<gfx::AnimationContainerTestApi>>
      test_apis_;
};

TEST_F(PasswordChangeRunProgressTest, SetProgressUpdatesCurrentStep) {
  EXPECT_EQ(run_progress()->GetCurrentProgressBarStep(),
            ProgressStep::PROGRESS_STEP_START);
  run_progress()->SetProgressBarStep(
      ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD);

  EXPECT_EQ(run_progress()->GetCurrentProgressBarStep(),
            ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD);
}

TEST_F(PasswordChangeRunProgressTest, CannotSetPriorProgressStep) {
  run_progress()->SetProgressBarStep(
      ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD);
  run_progress()->SetProgressBarStep(ProgressStep::PROGRESS_STEP_START);

  EXPECT_EQ(run_progress()->GetCurrentProgressBarStep(),
            ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD);
}

TEST_F(PasswordChangeRunProgressTest, SetProgressUpdatesPulsingStep) {
  absl::optional<ProgressStep> pulsing_step =
      run_progress()->GetPulsingProgressBarStep();
  ASSERT_TRUE(pulsing_step.has_value());
  EXPECT_EQ(pulsing_step.value(), ProgressStep::PROGRESS_STEP_START);
}

TEST_F(PasswordChangeRunProgressTest, IconsDoNotPulseSimultaneously) {
  // Changing the next step does not immediately change the pulsing icon - the
  // current one continues to pulse for up to one cycle.
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);
  run_progress()->SetProgressBarStep(
      ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD);
  absl::optional<ProgressStep> pulsing_step =
      run_progress()->GetPulsingProgressBarStep();
  ASSERT_TRUE(pulsing_step.has_value());
  EXPECT_EQ(pulsing_step.value(), ProgressStep::PROGRESS_STEP_START);

  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  pulsing_step = run_progress()->GetPulsingProgressBarStep();
  ASSERT_TRUE(pulsing_step.has_value());
  EXPECT_EQ(pulsing_step.value(), ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD);
}

TEST_F(PasswordChangeRunProgressTest, PauseAndResumeIconPulsing) {
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);
  EXPECT_TRUE(run_progress()->GetPulsingProgressBarStep().has_value());

  run_progress()->PauseIconAnimation();
  // It does not stop immediately:
  EXPECT_TRUE(run_progress()->GetPulsingProgressBarStep().has_value());

  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_FALSE(run_progress()->GetPulsingProgressBarStep());

  run_progress()->ResumeIconAnimation();
  ASSERT_TRUE(run_progress()->GetPulsingProgressBarStep().has_value());
  EXPECT_EQ(run_progress()->GetPulsingProgressBarStep().value(),
            ProgressStep::PROGRESS_STEP_START);

  // After resuming, it runs continuously again.
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_TRUE(run_progress()->GetPulsingProgressBarStep().has_value());
}

TEST_F(PasswordChangeRunProgressTest, SetProgressMultipleTimes) {
  // Changing the next step does not immediately change the pulsing icon - the
  // current one continues to pulse for up to two cycles.
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);
  EXPECT_TRUE(run_progress()->GetPulsingProgressBarStep().has_value());

  // Advance shortly after another by two steps.
  run_progress()->SetProgressBarStep(
      ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD);
  run_progress()->SetProgressBarStep(ProgressStep::PROGRESS_STEP_SAVE_PASSWORD);
  EXPECT_EQ(run_progress()->GetCurrentProgressBarStep(),
            ProgressStep::PROGRESS_STEP_SAVE_PASSWORD);

  // In that case, the first icon will pulse 0.5 more times.
  EXPECT_EQ(run_progress()->GetPulsingProgressBarStep().value(),
            ProgressStep::PROGRESS_STEP_START);

  // The next icon will pulse exactly once because there is already a follow-up
  // step.
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_EQ(run_progress()->GetPulsingProgressBarStep().value(),
            ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD);

  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_EQ(run_progress()->GetPulsingProgressBarStep().value(),
            ProgressStep::PROGRESS_STEP_SAVE_PASSWORD);

  // This step continues to pulse.
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_EQ(run_progress()->GetPulsingProgressBarStep().value(),
            ProgressStep::PROGRESS_STEP_SAVE_PASSWORD);

  EXPECT_FALSE(run_progress()->IsCompleted());
}

TEST_F(PasswordChangeRunProgressTest,
       PauseWhileThePulsingStepIsNotTheCurrentOne) {
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);
  run_progress()->SetProgressBarStep(
      ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD);
  run_progress()->SetProgressBarStep(ProgressStep::PROGRESS_STEP_SAVE_PASSWORD);

  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_EQ(run_progress()->GetPulsingProgressBarStep().value(),
            ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD);

  // Pausing it will still cause the current step to stop pulsing and the next
  // steps exactly once.
  run_progress()->PauseIconAnimation();
  EXPECT_EQ(run_progress()->GetPulsingProgressBarStep().value(),
            ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD);
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_EQ(run_progress()->GetPulsingProgressBarStep().value(),
            ProgressStep::PROGRESS_STEP_SAVE_PASSWORD);
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_FALSE(run_progress()->GetPulsingProgressBarStep());
  EXPECT_FALSE(run_progress()->IsCompleted());

  // Restarting the current step leads to continuous pulsing.
  run_progress()->ResumeIconAnimation();
  EXPECT_EQ(run_progress()->GetPulsingProgressBarStep().value(),
            ProgressStep::PROGRESS_STEP_SAVE_PASSWORD);
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_EQ(run_progress()->GetPulsingProgressBarStep().value(),
            ProgressStep::PROGRESS_STEP_SAVE_PASSWORD);
}

TEST_F(PasswordChangeRunProgressTest, LastIconPulsesOnceAndNotifiesCallback) {
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);
  run_progress()->SetProgressBarStep(
      ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD);
  run_progress()->SetProgressBarStep(ProgressStep::PROGRESS_STEP_SAVE_PASSWORD);

  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_EQ(run_progress()->GetPulsingProgressBarStep().value(),
            ProgressStep::PROGRESS_STEP_SAVE_PASSWORD);

  EXPECT_FALSE(run_progress()->IsCompleted());
  base::MockCallback<base::OnceClosure> closure;
  run_progress()->SetAnimationEndedCallback(closure.Get());
  run_progress()->SetProgressBarStep(ProgressStep::PROGRESS_STEP_END);

  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_EQ(run_progress()->GetPulsingProgressBarStep().value(),
            ProgressStep::PROGRESS_STEP_END);
  EXPECT_FALSE(run_progress()->IsCompleted());

  // The last icon only blinks once.
  EXPECT_CALL(closure, Run);
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_FALSE(run_progress()->GetPulsingProgressBarStep().has_value());
  EXPECT_TRUE(run_progress()->IsCompleted());
}
