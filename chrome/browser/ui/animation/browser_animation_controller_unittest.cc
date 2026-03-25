// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/animation/browser_animation_controller.h"

#include <memory>
#include <type_traits>
#include <vector>

#include "base/containers/map_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/animation/browser_animation_provider.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/identifier/unique_identifier.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/animation/tween.h"

namespace {

DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationGroup,
                                     kTestAnimationGroup);
DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationMotion,
                                     kTestAnimationMotion1);
DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationMotion,
                                     kTestAnimationMotion2);
DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationGroup,
                                     kTestAnimationGroup2);
DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationSequence,
                                     kTestAnimationSequence1);
DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationSequence,
                                     kTestAnimationSequence2);
DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationSequence,
                                     kTestAnimationSequence3);
DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationSequence,
                                     kTestAnimationSequence4);

class TestAnimationProvider : public CachingBrowserAnimationProvider {
 public:
  GroupInfos GenerateAnimations() const override {
    return Groups(
        Group(
            kTestAnimationGroup,
            Motion(
                kTestAnimationMotion1,
                Sequence(kTestAnimationSequence1, Keyframe(AtMs(0), Value(0.0)),
                         Keyframe(AtMs(200), Value(0.5),
                                  gfx::Tween::FAST_OUT_LINEAR_IN),
                         Keyframe(AtMs(600), Value(0.5)),
                         Keyframe(AtMs(1000), Value(1.0))),
                Sequence(kTestAnimationSequence2, StartingValue(1.0),
                         Segment(StartMs(300), LengthMs(300), ToValue(0.7),
                                 gfx::Tween::EASE_IN_OUT),
                         Segment(StartMs(700), EndMs(800), ToValue(1.0),
                                 gfx::Tween::EASE_IN_OUT_2),
                         Segment(StartMs(800), EndMs(900), ToValue(0.0),
                                 gfx::Tween::EASE_OUT))),
            Motion(
                kTestAnimationMotion2, TotalDurationMs(400), gfx::Tween::LINEAR,
                Animate(kTestAnimationSequence1, FromValue(1.0), ToValue(0.0)),
                Animate(kTestAnimationSequence2, FromValue(0.0),
                        ToValue(1.0)))),
        Group(kTestAnimationGroup2,
              Motion(kTestAnimationMotion2, TotalDurationMs(1000),
                     gfx::Tween::EASE_IN_OUT,
                     // Snap at 50%.
                     Snap(kTestAnimationSequence1, FromValue(0.0), ToValue(1.0),
                          AtPercent(0.5)),
                     // Snap at 0%.
                     Snap(kTestAnimationSequence2, FromValue(0.0), ToValue(1.0),
                          AtPercent(0.0)),
                     // Animate smoothly according to the global tween.
                     Animate(kTestAnimationSequence3, FromValue(0.0),
                             ToValue(1.0)),
                     // Snap at 300ms.
                     Snap(kTestAnimationSequence4, FromValue(0.0), ToValue(1.0),
                          AtMs(300)))));
  }
};

class TestAnimationProviderOverride : public BrowserAnimationProvider {
 public:
  std::optional<MotionSpecification> GetMotionSpecification(
      BrowserAnimationGroup group,
      BrowserAnimationMotion motion) const override {
    if (group != kTestAnimationGroup) {
      return std::nullopt;
    }
    if (motion == kTestAnimationMotion2) {
      return Motion(
          Sequence(kTestAnimationSequence1, Keyframe(AtMs(0), Value(1.0)),
                   Keyframe(AtMs(600), Value(0.0), gfx::Tween::EASE_OUT_2)),
          Sequence(kTestAnimationSequence2, StartingValue(0.0),
                   Segment(StartMs(0), LengthMs(300), ToValue(1.0),
                           gfx::Tween::LINEAR)));
    }
    return std::nullopt;
  }
};

}  // namespace

class BrowserAnimationControllerTest : public testing::Test {
 public:
  BrowserAnimationControllerTest() {
    EXPECT_CALL(browser_window_, GetUnownedUserDataHost)
        .WillRepeatedly(testing::ReturnRef(data_host_));
    controller_ = std::make_unique<BrowserAnimationController>(browser_window_);
    animation_provider_ = controller_->AddAnimationProvider(
        std::make_unique<TestAnimationProvider>());
  }

  ~BrowserAnimationControllerTest() override = default;

  BrowserAnimationController* controller() { return controller_.get(); }
  BrowserWindowInterface& browser_window() { return browser_window_; }
  base::test::TaskEnvironment& task_environment() { return task_environment_; }

  void RemoveProvider() {
    if (animation_provider_) {
      auto ptr = controller_->RemoveProviderForTesting(animation_provider_);
      animation_provider_ = nullptr;
    }
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ui::UnownedUserDataHost data_host_;
  MockBrowserWindowInterface browser_window_;
  std::unique_ptr<BrowserAnimationController> controller_;
  raw_ptr<TestAnimationProvider> animation_provider_ = nullptr;
};

TEST_F(BrowserAnimationControllerTest, Retrieve) {
  CHECK_EQ(controller(), BrowserAnimationController::From(&browser_window()));
}

TEST_F(BrowserAnimationControllerTest, AddAnimationProvider) {
  const auto motion_spec = controller()->GetMotionSpecificationForTesting(
      kTestAnimationGroup, kTestAnimationMotion1);
  ASSERT_TRUE(motion_spec.has_value());
  EXPECT_EQ(base::Milliseconds(1000), motion_spec->GetDuration());

  const auto t = [](int ms) {
    return internal::BrowserAnimationTime{base::Milliseconds(ms)};
  };

  const auto* el1_spec =
      base::FindOrNull(motion_spec->sequences, kTestAnimationSequence1);
  ASSERT_NE(el1_spec, nullptr);
  ASSERT_EQ(4U, el1_spec->keyframes.size());

  EXPECT_EQ(0.0, el1_spec->keyframes[0].value);
  EXPECT_EQ(t(0), el1_spec->keyframes[0].time);

  EXPECT_EQ(0.5, el1_spec->keyframes[1].value);
  EXPECT_EQ(t(200), el1_spec->keyframes[1].time);
  EXPECT_EQ(gfx::Tween::FAST_OUT_LINEAR_IN, el1_spec->keyframes[1].tween_type);

  EXPECT_EQ(0.5, el1_spec->keyframes[2].value);
  EXPECT_EQ(t(600), el1_spec->keyframes[2].time);

  EXPECT_EQ(1.0, el1_spec->keyframes[3].value);
  EXPECT_EQ(t(1000), el1_spec->keyframes[3].time);
  EXPECT_EQ(gfx::Tween::LINEAR, el1_spec->keyframes[3].tween_type);

  const auto* el2_spec =
      base::FindOrNull(motion_spec->sequences, kTestAnimationSequence2);
  ASSERT_NE(el2_spec, nullptr);
  ASSERT_EQ(6U, el2_spec->keyframes.size());

  EXPECT_EQ(1.0, el2_spec->keyframes[0].value);
  EXPECT_EQ(t(0), el2_spec->keyframes[0].time);

  EXPECT_EQ(1.0, el2_spec->keyframes[1].value);
  EXPECT_EQ(t(300), el2_spec->keyframes[1].time);

  EXPECT_EQ(0.7, el2_spec->keyframes[2].value);
  EXPECT_EQ(t(600), el2_spec->keyframes[2].time);
  EXPECT_EQ(gfx::Tween::EASE_IN_OUT, el2_spec->keyframes[2].tween_type);

  EXPECT_EQ(0.7, el2_spec->keyframes[3].value);
  EXPECT_EQ(t(700), el2_spec->keyframes[3].time);

  EXPECT_EQ(1.0, el2_spec->keyframes[4].value);
  EXPECT_EQ(t(800), el2_spec->keyframes[4].time);
  EXPECT_EQ(gfx::Tween::EASE_IN_OUT_2, el2_spec->keyframes[4].tween_type);

  EXPECT_EQ(0.0, el2_spec->keyframes[5].value);
  EXPECT_EQ(t(900), el2_spec->keyframes[5].time);
  EXPECT_EQ(gfx::Tween::EASE_OUT, el2_spec->keyframes[5].tween_type);
}

TEST_F(BrowserAnimationControllerTest, RemoveProviderForTesting) {
  RemoveProvider();
  const auto motion_spec = controller()->GetMotionSpecificationForTesting(
      kTestAnimationGroup, kTestAnimationMotion1);
  ASSERT_FALSE(motion_spec.has_value());
}

TEST_F(BrowserAnimationControllerTest, SecondProvider) {
  controller()->AddAnimationProvider(
      std::make_unique<TestAnimationProviderOverride>());
  auto motion_spec = controller()->GetMotionSpecificationForTesting(
      kTestAnimationGroup, kTestAnimationMotion2);
  ASSERT_TRUE(motion_spec.has_value());
  EXPECT_EQ(2U, motion_spec->sequences.size());
  EXPECT_EQ(base::Milliseconds(600), motion_spec->GetDuration());
  EXPECT_EQ(2U,
            motion_spec->sequences[kTestAnimationSequence2].keyframes.size());
}

TEST_F(BrowserAnimationControllerTest, IsAnimating) {
  EXPECT_FALSE(controller()->IsAnimating(kTestAnimationGroup));
  EXPECT_FALSE(controller()->IsAnimating(kTestAnimationGroup2));

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_TRUE(controller()->IsAnimating(kTestAnimationGroup));
  EXPECT_FALSE(controller()->IsAnimating(kTestAnimationGroup2));

  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_TRUE(controller()->IsAnimating(kTestAnimationGroup));
  EXPECT_FALSE(controller()->IsAnimating(kTestAnimationGroup2));

  task_environment().FastForwardBy(base::Milliseconds(600));
  EXPECT_FALSE(controller()->IsAnimating(kTestAnimationGroup));
  EXPECT_FALSE(controller()->IsAnimating(kTestAnimationGroup2));
}

TEST_F(BrowserAnimationControllerTest, GetCurrentMotion) {
  EXPECT_EQ(BrowserAnimationMotion(),
            controller()->GetCurrentMotion(kTestAnimationGroup));
  EXPECT_EQ(BrowserAnimationMotion(),
            controller()->GetCurrentMotion(kTestAnimationGroup2));

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(kTestAnimationMotion1,
            controller()->GetCurrentMotion(kTestAnimationGroup));
  EXPECT_EQ(BrowserAnimationMotion(),
            controller()->GetCurrentMotion(kTestAnimationGroup2));

  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(kTestAnimationMotion1,
            controller()->GetCurrentMotion(kTestAnimationGroup));
  EXPECT_EQ(BrowserAnimationMotion(),
            controller()->GetCurrentMotion(kTestAnimationGroup2));

  task_environment().FastForwardBy(base::Milliseconds(600));
  EXPECT_EQ(BrowserAnimationMotion(),
            controller()->GetCurrentMotion(kTestAnimationGroup));
  EXPECT_EQ(BrowserAnimationMotion(),
            controller()->GetCurrentMotion(kTestAnimationGroup2));
}

TEST_F(BrowserAnimationControllerTest, StartAndCancel) {
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  controller()->Cancel(kTestAnimationGroup);
  EXPECT_FALSE(controller()->IsAnimating(kTestAnimationGroup));
  EXPECT_FALSE(controller()->IsAnimating(kTestAnimationGroup2));
}

TEST_F(BrowserAnimationControllerTest, GetCurrentValueSegmentsAndKeyframes) {
  EXPECT_EQ(std::nullopt, controller()->GetCurrentValue(
                              kTestAnimationGroup, kTestAnimationSequence1));
  EXPECT_EQ(std::nullopt, controller()->GetCurrentValue(
                              kTestAnimationGroup, kTestAnimationSequence2));

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));

  task_environment().FastForwardBy(base::Milliseconds(100));
  EXPECT_GT(controller()->GetCurrentValue(kTestAnimationGroup,
                                          kTestAnimationSequence1),
            0.0);
  EXPECT_LT(controller()->GetCurrentValue(kTestAnimationGroup,
                                          kTestAnimationSequence1),
            0.5);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));

  task_environment().FastForwardBy(base::Milliseconds(400));
  EXPECT_EQ(0.5, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_LT(controller()->GetCurrentValue(kTestAnimationGroup,
                                          kTestAnimationSequence2),
            1.0);
  EXPECT_GT(controller()->GetCurrentValue(kTestAnimationGroup,
                                          kTestAnimationSequence2),
            0.7);

  task_environment().FastForwardBy(base::Milliseconds(350));
  EXPECT_GT(controller()->GetCurrentValue(kTestAnimationGroup,
                                          kTestAnimationSequence1),
            0.5);
  EXPECT_LT(controller()->GetCurrentValue(kTestAnimationGroup,
                                          kTestAnimationSequence1),
            1.0);
  EXPECT_GT(controller()->GetCurrentValue(kTestAnimationGroup,
                                          kTestAnimationSequence2),
            0.0);
  EXPECT_LT(controller()->GetCurrentValue(kTestAnimationGroup,
                                          kTestAnimationSequence2),
            1.0);

  task_environment().FastForwardBy(base::Milliseconds(300));
  EXPECT_EQ(std::nullopt, controller()->GetCurrentValue(
                              kTestAnimationGroup, kTestAnimationSequence1));
  EXPECT_EQ(std::nullopt, controller()->GetCurrentValue(
                              kTestAnimationGroup, kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, GetCurrentValueSnapAndAnimate) {
  controller()->Start(kTestAnimationGroup2, kTestAnimationMotion2);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence2));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence3));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence4));

  // Fast-forward to 50ms.
  task_environment().FastForwardBy(base::Milliseconds(50));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence2));
  // This is on the global curve, so it should start to move.
  EXPECT_GT(controller()->GetCurrentValue(kTestAnimationGroup2,
                                          kTestAnimationSequence3),
            0.0);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence4));

  // Fast-forward to 250ms.
  task_environment().FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence2));
  // The animation is ease in/out, so it will be slower than linear.
  EXPECT_GT(controller()->GetCurrentValue(kTestAnimationGroup2,
                                          kTestAnimationSequence3),
            0.0);
  EXPECT_LT(controller()->GetCurrentValue(kTestAnimationGroup2,
                                          kTestAnimationSequence3),
            0.25);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence4));

  // Fast-forward to 350ms.
  task_environment().FastForwardBy(base::Milliseconds(100));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence2));
  EXPECT_GT(controller()->GetCurrentValue(kTestAnimationGroup2,
                                          kTestAnimationSequence3),
            0.0);
  EXPECT_LT(controller()->GetCurrentValue(kTestAnimationGroup2,
                                          kTestAnimationSequence3),
            0.35);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence4));

  // Fast-forward to 550ms.
  task_environment().FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence2));
  // This is around halfway through the curve.
  EXPECT_GT(controller()->GetCurrentValue(kTestAnimationGroup2,
                                          kTestAnimationSequence3),
            0.25);
  EXPECT_LT(controller()->GetCurrentValue(kTestAnimationGroup2,
                                          kTestAnimationSequence3),
            0.75);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence4));

  // Fast-forward to 800ms.
  task_environment().FastForwardBy(base::Milliseconds(250));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence2));
  // During the back half of the curve, the animation should be ahead of linear.
  EXPECT_GT(controller()->GetCurrentValue(kTestAnimationGroup2,
                                          kTestAnimationSequence3),
            0.75);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence4));
}

TEST_F(BrowserAnimationControllerTest, GetRemainingTime) {
  EXPECT_EQ(base::Milliseconds(0),
            controller()->GetMotionDuration(kTestAnimationGroup));

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(base::Milliseconds(1000),
            controller()->GetMotionDuration(kTestAnimationGroup));

  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(base::Milliseconds(1000),
            controller()->GetMotionDuration(kTestAnimationGroup));

  task_environment().FastForwardBy(base::Milliseconds(600));
  EXPECT_EQ(base::Milliseconds(0),
            controller()->GetMotionDuration(kTestAnimationGroup));

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion2);
  EXPECT_EQ(base::Milliseconds(400),
            controller()->GetMotionDuration(kTestAnimationGroup));
}

TEST_F(BrowserAnimationControllerTest, StartDifferentMotionSameGroup) {
  // Start the other animation immediately.
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion2);
  EXPECT_EQ(kTestAnimationMotion2,
            controller()->GetCurrentMotion(kTestAnimationGroup));
  // Start the first animation again after a delay.
  task_environment().FastForwardBy(base::Milliseconds(250));
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(kTestAnimationMotion1,
            controller()->GetCurrentMotion(kTestAnimationGroup));
}

TEST_F(BrowserAnimationControllerTest, StartDifferentMotionDifferentGroup) {
  // Start the other animation immediately.
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion2);
  controller()->Start(kTestAnimationGroup2, kTestAnimationMotion2);
  EXPECT_EQ(kTestAnimationMotion2,
            controller()->GetCurrentMotion(kTestAnimationGroup));
  EXPECT_EQ(kTestAnimationMotion2,
            controller()->GetCurrentMotion(kTestAnimationGroup2));

  // Fast-forward until one animation finishes.
  task_environment().FastForwardBy(base::Milliseconds(500));
  EXPECT_EQ(BrowserAnimationMotion(),
            controller()->GetCurrentMotion(kTestAnimationGroup));
  EXPECT_EQ(kTestAnimationMotion2,
            controller()->GetCurrentMotion(kTestAnimationGroup2));
}

TEST_F(BrowserAnimationControllerTest, Subscribe) {
  UNCALLED_MOCK_CALLBACK(BrowserAnimationCallback, callback);
  const auto subscription =
      controller()->Subscribe(kTestAnimationGroup, callback.Get());

  EXPECT_CALL_IN_SCOPE(
      callback, Run(controller(), BrowserAnimationUpdate::kStarted),
      controller()->Start(kTestAnimationGroup, kTestAnimationMotion1));

  EXPECT_CALL(callback, Run(controller(), BrowserAnimationUpdate::kProgressed))
      .Times(testing::AtLeast(1));
  task_environment().FastForwardBy(base::Milliseconds(500));

  {
    testing::InSequence in_sequence;
    EXPECT_CALL(callback,
                Run(controller(), BrowserAnimationUpdate::kProgressed))
        .Times(testing::AtLeast(1));
    EXPECT_CALL(callback, Run(controller(), BrowserAnimationUpdate::kEnded))
        .Times(1);
    task_environment().FastForwardBy(base::Milliseconds(600));
  }

  EXPECT_CALL(callback, Run(controller(), testing::_)).Times(0);
  task_environment().FastForwardBy(base::Milliseconds(500));
}

TEST_F(BrowserAnimationControllerTest, AtEndOfAnimationDuringFinalCallback) {
  std::vector<double> values;
  bool ended = false;
  UNCALLED_MOCK_CALLBACK(BrowserAnimationCallback, callback);
  const auto subscription =
      controller()->Subscribe(kTestAnimationGroup, callback.Get());

  EXPECT_CALL_IN_SCOPE(
      callback, Run(controller(), BrowserAnimationUpdate::kStarted),
      controller()->Start(kTestAnimationGroup, kTestAnimationMotion1));

  EXPECT_CALL(callback, Run)
      .WillRepeatedly([&](const BrowserAnimationController* controller,
                          BrowserAnimationUpdate status) {
        const auto result = controller->GetCurrentValue(
            kTestAnimationGroup, kTestAnimationSequence1);
        ASSERT_TRUE(result.has_value());
        ASSERT_FALSE(ended);
        values.push_back(result.value());
        if (status == BrowserAnimationUpdate::kEnded) {
          ended = true;
        }
      });
  task_environment().FastForwardBy(base::Milliseconds(500));
  task_environment().FastForwardBy(base::Milliseconds(600));
  task_environment().FastForwardBy(base::Milliseconds(500));

  ASSERT_GT(values.size(), 0U);
  EXPECT_TRUE(ended);
  EXPECT_EQ(1.0, values.back());
}

TEST_F(BrowserAnimationControllerTest, AnimationCanceledCallback) {
  std::vector<double> values;
  bool canceled = false;
  UNCALLED_MOCK_CALLBACK(BrowserAnimationCallback, callback);
  const auto subscription =
      controller()->Subscribe(kTestAnimationGroup, callback.Get());

  EXPECT_CALL_IN_SCOPE(
      callback, Run(controller(), BrowserAnimationUpdate::kStarted),
      controller()->Start(kTestAnimationGroup, kTestAnimationMotion1));

  EXPECT_CALL(callback, Run)
      .WillRepeatedly([&](const BrowserAnimationController* controller,
                          BrowserAnimationUpdate status) {
        ASSERT_FALSE(canceled);
        if (status == BrowserAnimationUpdate::kCanceled) {
          const auto result = controller->GetCurrentValue(
              kTestAnimationGroup, kTestAnimationSequence1);
          ASSERT_FALSE(result.has_value());
          canceled = true;
        } else {
          const auto result = controller->GetCurrentValue(
              kTestAnimationGroup, kTestAnimationSequence1);
          ASSERT_TRUE(result.has_value());
          values.push_back(result.value());
        }
      });
  task_environment().FastForwardBy(base::Milliseconds(500));
  controller()->Cancel(kTestAnimationGroup);

  ASSERT_GT(values.size(), 0U);
  EXPECT_TRUE(canceled);
  EXPECT_NE(1.0, values.back());
}

TEST_F(BrowserAnimationControllerTest, AnimationRestartedCallbacks) {
  UNCALLED_MOCK_CALLBACK(BrowserAnimationCallback, callback);
  const auto subscription =
      controller()->Subscribe(kTestAnimationGroup, callback.Get());

  EXPECT_CALL_IN_SCOPE(
      callback, Run(controller(), BrowserAnimationUpdate::kStarted),
      controller()->Start(kTestAnimationGroup, kTestAnimationMotion1));

  EXPECT_CALL(callback, Run(controller(), BrowserAnimationUpdate::kProgressed))
      .Times(testing::AtLeast(1));
  task_environment().FastForwardBy(base::Milliseconds(500));

  EXPECT_CALLS_IN_SCOPE_2(
      callback, Run(controller(), BrowserAnimationUpdate::kCanceled), callback,
      Run(controller(), BrowserAnimationUpdate::kStarted),
      controller()->Start(kTestAnimationGroup, kTestAnimationMotion2));

  EXPECT_CALL(callback, Run(controller(), BrowserAnimationUpdate::kProgressed))
      .Times(testing::AtLeast(1));
  task_environment().FastForwardBy(base::Milliseconds(200));
}
