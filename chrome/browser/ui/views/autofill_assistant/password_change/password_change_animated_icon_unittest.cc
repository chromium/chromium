// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_animated_icon.h"

#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autofill_assistant/password_change/vector_icons/vector_icons.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/test/views_test_base.h"

namespace {

constexpr int kIconSize = 16;

// A test class that allows overwriting the internal timer used for
// animations.
class TestPasswordChangeAnimatedIcon : public PasswordChangeAnimatedIcon {
 public:
  explicit TestPasswordChangeAnimatedIcon(
      autofill_assistant::password_change::ProgressStep progress_step)
      : PasswordChangeAnimatedIcon(/*id=*/0, progress_step) {}
  ~TestPasswordChangeAnimatedIcon() override = default;

  void AnimationContainerWasSet(gfx::AnimationContainer* container) override {
    PasswordChangeAnimatedIcon::AnimationContainerWasSet(container);
    container_test_api_.reset();
    if (container) {
      container_test_api_ =
          std::make_unique<gfx::AnimationContainerTestApi>(container);
    }
  }

  gfx::AnimationContainerTestApi* test_api() {
    return container_test_api_.get();
  }

 private:
  std::unique_ptr<gfx::AnimationContainerTestApi> container_test_api_;
};

class PasswordChangeAnimatedIconTest : public views::ViewsTestBase {
 public:
  PasswordChangeAnimatedIconTest() = default;
  ~PasswordChangeAnimatedIconTest() override = default;

  void SetUp() override {
    views::ViewsTestBase::SetUp();
    widget_ = CreateTestWidget();
    CreateIcon();
  }

  void TearDown() override {
    widget_.reset();
    views::ViewsTestBase::TearDown();
  }

  void CreateIcon(autofill_assistant::password_change::ProgressStep
                      progress_step = autofill_assistant::password_change::
                          ProgressStep::PROGRESS_STEP_CHANGE_PASSWORD) {
    animated_icon_ = widget_->SetContentsView(
        std::make_unique<TestPasswordChangeAnimatedIcon>(progress_step));
  }

  TestPasswordChangeAnimatedIcon* animated_icon() {
    return animated_icon_.get();
  }

  void AdvanceTime(base::TimeDelta time) {
    animated_icon()->test_api()->IncrementTime(time);
  }

 private:
  // Widget to anchor the view and retrieve a color provider from.
  std::unique_ptr<views::Widget> widget_;

  // The object to be tested.
  raw_ptr<TestPasswordChangeAnimatedIcon> animated_icon_ = nullptr;
};

TEST_F(PasswordChangeAnimatedIconTest, SetsCorrectIcon) {
  CreateIcon(
      autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_START);
  EXPECT_EQ(
      animated_icon()->GetImageModel(),
      ui::ImageModel::FromVectorIcon(
          autofill_assistant::password_change::kPasswordChangeProgressStartIcon,
          ui::kColorIconDisabled, kIconSize));

  CreateIcon(autofill_assistant::password_change::ProgressStep::
                 PROGRESS_STEP_CHANGE_PASSWORD);
  EXPECT_EQ(animated_icon()->GetImageModel(),
            ui::ImageModel::FromVectorIcon(vector_icons::kSettingsIcon,
                                           ui::kColorIconDisabled, kIconSize));

  CreateIcon(autofill_assistant::password_change::ProgressStep::
                 PROGRESS_STEP_SAVE_PASSWORD);
  EXPECT_EQ(animated_icon()->GetImageModel(),
            ui::ImageModel::FromVectorIcon(kKeyIcon, ui::kColorIconDisabled,
                                           kIconSize));
}

TEST_F(PasswordChangeAnimatedIconTest, StartPulsingAnimation) {
  // The icon is not pulsing after creation.
  EXPECT_FALSE(animated_icon()->IsPulsing());

  animated_icon()->StartPulsingAnimation();
  EXPECT_TRUE(animated_icon()->IsPulsing());
}

TEST_F(PasswordChangeAnimatedIconTest, PausePulsingAnimation) {
  animated_icon()->StartPulsingAnimation();
  EXPECT_TRUE(animated_icon()->IsPulsing());

  // The icon continues to pulse for at least one cycle after pausing.
  animated_icon()->StopPulsingAnimation();
  EXPECT_TRUE(animated_icon()->IsPulsing());

  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);
  EXPECT_TRUE(animated_icon()->IsPulsing());

  // But after at most 2 cycle durations, it stops.
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_FALSE(animated_icon()->IsPulsing());
}

TEST_F(PasswordChangeAnimatedIconTest, ResumePulsingAnimation) {
  animated_icon()->StartPulsingAnimation();
  EXPECT_TRUE(animated_icon()->IsPulsing());

  animated_icon()->StopPulsingAnimation();
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_FALSE(animated_icon()->IsPulsing());

  animated_icon()->StartPulsingAnimation();
  EXPECT_TRUE(animated_icon()->IsPulsing());
}

TEST_F(PasswordChangeAnimatedIconTest, StartPulsingAnimationTwice) {
  animated_icon()->StartPulsingAnimation();
  EXPECT_TRUE(animated_icon()->IsPulsing());

  // Calling it again still leaves it pulsing.
  animated_icon()->StartPulsingAnimation();
  EXPECT_TRUE(animated_icon()->IsPulsing());

  animated_icon()->StopPulsingAnimation();
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);

  // Restarting it now avoids it from ever stopping.
  animated_icon()->StartPulsingAnimation();
  EXPECT_TRUE(animated_icon()->IsPulsing());

  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_TRUE(animated_icon()->IsPulsing());
}

TEST_F(PasswordChangeAnimatedIconTest, PulseOnce) {
  animated_icon()->StartPulsingAnimation(/*pulse_once=*/true);
  EXPECT_TRUE(animated_icon()->IsPulsing());

  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);
  EXPECT_TRUE(animated_icon()->IsPulsing());

  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_FALSE(animated_icon()->IsPulsing());
}

TEST_F(PasswordChangeAnimatedIconTest, CallbackSetDuringPulsing) {
  animated_icon()->StartPulsingAnimation();

  base::MockOnceClosure closure;
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);
  animated_icon()->SetAnimationEndedCallback(closure.Get());
  animated_icon()->StopPulsingAnimation();

  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_CALL(closure, Run);
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_FALSE(animated_icon()->IsPulsing());
}

TEST_F(PasswordChangeAnimatedIconTest, CallbackSetBeforeStartingPulsing) {
  base::MockOnceClosure closure;
  animated_icon()->SetAnimationEndedCallback(closure.Get());

  animated_icon()->StartPulsingAnimation();
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);
  animated_icon()->StopPulsingAnimation();

  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_CALL(closure, Run);
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_FALSE(animated_icon()->IsPulsing());
}

}  // namespace
