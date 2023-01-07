// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_assistant/password_change/password_change_animated_icon.h"

#include "base/time/time.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autofill_assistant/password_change/vector_icons/vector_icons.h"
#include "components/autofill_assistant/browser/public/password_change/proto/actions.pb.h"
#include "components/vector_icons/vector_icons.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/test/views_test_base.h"

namespace gfx {
class AnimationContainer;
}  // namespace gfx

namespace {

constexpr int kIconSize = 16;

class PasswordChangeAnimatedIconTest
    : public views::ViewsTestBase,
      public PasswordChangeAnimatedIcon::Delegate {
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
    animated_icon_ =
        widget_->SetContentsView(std::make_unique<PasswordChangeAnimatedIcon>(
            /*id=*/0, progress_step, this));
  }

  // PasswordChangeAnimatedIcon::Delegate:
  void OnAnimationContainerWasSet(PasswordChangeAnimatedIcon* icon,
                                  gfx::AnimationContainer* container) override {
    container_test_api_.reset();
    if (container) {
      container_test_api_ =
          std::make_unique<gfx::AnimationContainerTestApi>(container);
    }
  }
  MOCK_METHOD(void,
              OnAnimationEnded,
              (PasswordChangeAnimatedIcon*),
              (override));

  PasswordChangeAnimatedIcon* animated_icon() { return animated_icon_.get(); }

  void AdvanceTime(base::TimeDelta time) {
    if (container_test_api_)
      container_test_api_->IncrementTime(time);
  }

 private:
  // Widget to anchor the view and retrieve a color provider from.
  std::unique_ptr<views::Widget> widget_;
  // A test API to control time for the animations.
  std::unique_ptr<gfx::AnimationContainerTestApi> container_test_api_;

  // The object to be tested.
  raw_ptr<PasswordChangeAnimatedIcon> animated_icon_ = nullptr;
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

  animated_icon()->StopPulsingAnimation();
  EXPECT_TRUE(animated_icon()->IsPulsing());

  // The icon will complete its current cycle.
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);
  EXPECT_TRUE(animated_icon()->IsPulsing());

  // The icon stops after a full cycle.
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_FALSE(animated_icon()->IsPulsing());
}

TEST_F(PasswordChangeAnimatedIconTest, ResumePulsingAnimation) {
  animated_icon()->StartPulsingAnimation();
  EXPECT_TRUE(animated_icon()->IsPulsing());

  animated_icon()->StopPulsingAnimation();
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

  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);

  animated_icon()->StopPulsingAnimation();
  EXPECT_CALL(*this, OnAnimationEnded(animated_icon()));
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_FALSE(animated_icon()->IsPulsing());
}

TEST_F(PasswordChangeAnimatedIconTest, CallbackSetBeforeStartingPulsing) {
  animated_icon()->StartPulsingAnimation();
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);

  animated_icon()->StopPulsingAnimation();
  EXPECT_CALL(*this, OnAnimationEnded(animated_icon()));
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_FALSE(animated_icon()->IsPulsing());
}

TEST_F(PasswordChangeAnimatedIconTest, CallbackCalledMultipleTimes) {
  animated_icon()->StartPulsingAnimation();
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);

  animated_icon()->StopPulsingAnimation();
  EXPECT_CALL(*this, OnAnimationEnded(animated_icon()));
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_FALSE(animated_icon()->IsPulsing());

  animated_icon()->StartPulsingAnimation();
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration / 2);

  animated_icon()->StopPulsingAnimation();
  EXPECT_CALL(*this, OnAnimationEnded(animated_icon()));
  AdvanceTime(PasswordChangeAnimatedIcon::kAnimationDuration);
  EXPECT_FALSE(animated_icon()->IsPulsing());
}

}  // namespace
