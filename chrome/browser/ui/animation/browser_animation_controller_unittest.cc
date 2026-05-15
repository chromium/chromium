// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/animation/browser_animation_controller.h"

#include <concepts>
#include <memory>
#include <type_traits>
#include <vector>

#include "base/containers/map_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/animation/browser_animation_provider.h"
#include "chrome/browser/ui/animation/browser_animation_provider_internal.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/identifier/unique_identifier.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/safe_castable.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/animation/animation_test_api.h"
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

#define EXPECT_BETWEEN(V, Lo, Hi) \
  EXPECT_GT(V, Lo);               \
  EXPECT_LT(V, Hi)

class TestAnimationProvider : public CachingBrowserAnimationProvider {
 public:
  DECLARE_SAFE_CAST_TARGET()
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

DEFINE_SAFE_CAST_TARGET(TestAnimationProvider)

class TestAnimationProviderOverride : public BrowserAnimationProvider {
 public:
  DECLARE_SAFE_CAST_TARGET()

  std::optional<MotionSpecification> GetMotionSpecificationImpl(
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

DEFINE_SAFE_CAST_TARGET(TestAnimationProviderOverride)

}  // namespace

class BrowserAnimationControllerTest : public testing::Test {
 public:
  BrowserAnimationControllerTest() {
    EXPECT_CALL(browser_window_, GetUnownedUserDataHost)
        .WillRepeatedly(testing::ReturnRef(data_host_));
    controller_ = std::make_unique<BrowserAnimationController>(browser_window_);
    SetProvider<TestAnimationProvider>();
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

  template <typename T>
    requires std::derived_from<T, BrowserAnimationProvider>
  T* SetProvider() {
    RemoveProvider();
    auto t_ptr = std::make_unique<T>();
    T* const t = t_ptr.get();
    animation_provider_ = controller_->AddAnimationProvider(std::move(t_ptr));
    return t;
  }

  void FastForwardMs(int ms) {
    task_environment_.FastForwardBy(base::Milliseconds(ms));
  }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  ui::UnownedUserDataHost data_host_;
  MockBrowserWindowInterface browser_window_;
  std::unique_ptr<BrowserAnimationController> controller_;
  raw_ptr<BrowserAnimationProvider> animation_provider_ = nullptr;
};

TEST_F(BrowserAnimationControllerTest, Retrieve) {
  CHECK_EQ(controller(), BrowserAnimationController::From(&browser_window()));
}

TEST_F(BrowserAnimationControllerTest, RetrieveProvider) {
  const auto* const provider = SetProvider<TestAnimationProvider>();
  EXPECT_EQ(provider,
            controller()->GetAnimationProvider<TestAnimationProvider>());
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

  FastForwardMs(500);
  EXPECT_TRUE(controller()->IsAnimating(kTestAnimationGroup));
  EXPECT_FALSE(controller()->IsAnimating(kTestAnimationGroup2));

  FastForwardMs(600);
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

  FastForwardMs(500);
  EXPECT_EQ(kTestAnimationMotion1,
            controller()->GetCurrentMotion(kTestAnimationGroup));
  EXPECT_EQ(BrowserAnimationMotion(),
            controller()->GetCurrentMotion(kTestAnimationGroup2));

  FastForwardMs(600);
  EXPECT_EQ(BrowserAnimationMotion(),
            controller()->GetCurrentMotion(kTestAnimationGroup));
  EXPECT_EQ(BrowserAnimationMotion(),
            controller()->GetCurrentMotion(kTestAnimationGroup2));
}

TEST_F(BrowserAnimationControllerTest, StartAndClear) {
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  controller()->Clear(kTestAnimationGroup);
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

  FastForwardMs(100);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1),
                 0.0, 0.5);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));

  FastForwardMs(400);
  EXPECT_EQ(0.5, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2),
                 0.7, 1.0);

  FastForwardMs(350);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1),
                 0.5, 1.0);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2),
                 0.0, 1.0);

  FastForwardMs(300);
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
  FastForwardMs(50);
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
  FastForwardMs(200);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence2));
  // The animation is ease in/out, so it will be slower than linear.
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence3),
                 0.0, 0.25);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence4));

  // Fast-forward to 350ms.
  FastForwardMs(100);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence2));
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence3),
                 0.0, 0.35);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence4));

  // Fast-forward to 550ms.
  FastForwardMs(200);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence2));
  // This is around halfway through the curve.
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence3),
                 0.25, 0.75);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence4));

  // Fast-forward to 800ms.
  FastForwardMs(250);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence2));
  // During the back half of the curve, the animation should be ahead of linear.
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence3),
                 0.75, 1.0);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup2,
                                               kTestAnimationSequence4));
}

TEST_F(BrowserAnimationControllerTest, GetRemainingTime) {
  EXPECT_EQ(base::Milliseconds(0),
            controller()->GetMotionDuration(kTestAnimationGroup));

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(base::Milliseconds(1000),
            controller()->GetMotionDuration(kTestAnimationGroup));

  FastForwardMs(500);
  EXPECT_EQ(base::Milliseconds(1000),
            controller()->GetMotionDuration(kTestAnimationGroup));

  FastForwardMs(600);
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
  FastForwardMs(250);
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
  FastForwardMs(500);
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
  FastForwardMs(500);

  {
    testing::InSequence in_sequence;
    EXPECT_CALL(callback,
                Run(controller(), BrowserAnimationUpdate::kProgressed))
        .Times(testing::AtLeast(1));
    EXPECT_CALL(callback, Run(controller(), BrowserAnimationUpdate::kEnded))
        .Times(1);
    FastForwardMs(600);
  }

  EXPECT_CALL(callback, Run(controller(), testing::_)).Times(0);
  FastForwardMs(500);
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
  FastForwardMs(500);
  FastForwardMs(600);
  FastForwardMs(500);

  ASSERT_GT(values.size(), 0U);
  EXPECT_TRUE(ended);
  EXPECT_EQ(1.0, values.back());
}

TEST_F(BrowserAnimationControllerTest, AnimationClearedCallback) {
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
  FastForwardMs(500);
  controller()->Clear(kTestAnimationGroup);

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
  FastForwardMs(500);

  EXPECT_CALLS_IN_SCOPE_2(
      callback, Run(controller(), BrowserAnimationUpdate::kCanceled), callback,
      Run(controller(), BrowserAnimationUpdate::kStarted),
      controller()->Start(kTestAnimationGroup, kTestAnimationMotion2));

  EXPECT_CALL(callback, Run(controller(), BrowserAnimationUpdate::kProgressed))
      .Times(testing::AtLeast(1));
  FastForwardMs(200);
}

TEST_F(BrowserAnimationControllerTest, RichAnimationOff) {
  const auto lock = gfx::AnimationTestApi::SetRichAnimationRenderMode(
      gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);

  UNCALLED_MOCK_CALLBACK(BrowserAnimationCallback, callback);
  const auto subscription =
      controller()->Subscribe(kTestAnimationGroup, callback.Get());

  EXPECT_CALLS_IN_SCOPE_2(
      callback, Run(controller(), BrowserAnimationUpdate::kStarted), callback,
      Run(controller(), BrowserAnimationUpdate::kEnded),
      controller()->Start(kTestAnimationGroup, kTestAnimationMotion1));
}

namespace {

class SingleGroupAnimationProvider : public CachingBrowserAnimationProvider {
 public:
  DECLARE_SAFE_CAST_TARGET()

  SingleGroupAnimationProvider() = default;
  ~SingleGroupAnimationProvider() override = default;

  GroupInfos GenerateAnimations() const override {
    MotionLookup lookup;
    for (auto& motion : GetMotions()) {
      lookup.emplace(std::move(motion));
    }
    return Groups(GroupInfo{kTestAnimationGroup, lookup});
  }

  template <typename... Args>
  void SetSequenceParams(const Args&... args) {
    CachingBrowserAnimationProvider::SetSequenceParams(kTestAnimationGroup,
                                                       args...);
  }

 protected:
  virtual std::vector<MotionInfo> GetMotions() const = 0;
};

DEFINE_SAFE_CAST_TARGET(SingleGroupAnimationProvider)

}  // namespace

TEST_F(BrowserAnimationControllerTest, Transition_NotParticipating) {
  class P : public SingleGroupAnimationProvider {
   public:
    std::vector<MotionInfo> GetMotions() const override {
      return {
          Motion(
              kTestAnimationMotion1, TotalDurationMs(1000), gfx::Tween::LINEAR,
              Animate(kTestAnimationSequence1, FromValue(0.0), ToValue(1.0)),
              Animate(kTestAnimationSequence2, FromValue(1.0), ToValue(0.0))),
          Motion(kTestAnimationMotion2, TotalDurationMs(1000),
                 gfx::Tween::LINEAR,
                 Sequence(kTestAnimationSequence1,
                          Keyframe(AtPercent(0.25), Value(0.25)),
                          Keyframe(AtPercent(0.75), Value(0.75))),
                 Sequence(kTestAnimationSequence2,
                          Keyframe(AtPercent(0.25), Value(0.25)),
                          Keyframe(AtPercent(0.75), Value(0.75))))};
    }
  };
  SetProvider<P>();

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
  FastForwardMs(1100);
  EXPECT_EQ(std::nullopt, controller()->GetCurrentValue(
                              kTestAnimationGroup, kTestAnimationSequence1));
  EXPECT_EQ(std::nullopt, controller()->GetCurrentValue(
                              kTestAnimationGroup, kTestAnimationSequence2));

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion2);
  EXPECT_EQ(0.25, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_EQ(0.25, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));
  FastForwardMs(1100);
  EXPECT_EQ(std::nullopt, controller()->GetCurrentValue(
                              kTestAnimationGroup, kTestAnimationSequence1));
  EXPECT_EQ(std::nullopt, controller()->GetCurrentValue(
                              kTestAnimationGroup, kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, Transition_Ignore) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() {
      SetSequenceParams(Persist(kTestAnimationSequence1),
                        Persist(kTestAnimationSequence2));
    }
    std::vector<MotionInfo> GetMotions() const override {
      return {
          Motion(kTestAnimationMotion1, TotalDurationMs(1000),
                 gfx::Tween::LINEAR,
                 Animate(kTestAnimationSequence1, Transition::kIgnoreOldValue,
                         FromValue(0.0), ToValue(1.0)),
                 Animate(kTestAnimationSequence2, Transition::kIgnoreOldValue,
                         FromValue(1.0), ToValue(0.0))),
          Motion(kTestAnimationMotion2, TotalDurationMs(1000),
                 gfx::Tween::LINEAR,
                 Sequence(kTestAnimationSequence1, Transition::kIgnoreOldValue,
                          Keyframe(AtPercent(0.25), Value(0.25)),
                          Keyframe(AtPercent(0.75), Value(0.75))),
                 Sequence(kTestAnimationSequence2, Transition::kIgnoreOldValue,
                          Keyframe(AtPercent(0.25), Value(0.25)),
                          Keyframe(AtPercent(0.75), Value(0.75))))};
    }
  };
  SetProvider<P>();

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
  FastForwardMs(1100);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion2);
  EXPECT_EQ(0.25, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_EQ(0.25, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));
  FastForwardMs(1100);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, Transition_StartAt_InsideRange) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() {
      SetSequenceParams(Persist(kTestAnimationSequence1),
                        Persist(kTestAnimationSequence2));
    }
    std::vector<MotionInfo> GetMotions() const override {
      return {
          Motion(
              kTestAnimationMotion1, TotalDurationMs(1000), gfx::Tween::LINEAR,
              Animate(kTestAnimationSequence1, FromValue(0.0), ToValue(1.0)),
              Animate(kTestAnimationSequence2, FromValue(1.0), ToValue(0.0))),
          Motion(kTestAnimationMotion2, TotalDurationMs(1000),
                 gfx::Tween::LINEAR,
                 Sequence(kTestAnimationSequence1,
                          Keyframe(AtPercent(0.25), Value(0.25)),
                          Keyframe(AtPercent(0.75), Value(0.75))),
                 Sequence(kTestAnimationSequence2,
                          Keyframe(AtPercent(0.25), Value(0.25)),
                          Keyframe(AtPercent(0.75), Value(0.75))))};
    }
  };
  SetProvider<P>();

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion2);
  FastForwardMs(1100);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));
  FastForwardMs(500);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1),
                 0.75, 1.0);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2),
                 0.0, 0.75);
  FastForwardMs(600);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, Transition_StartAt_OutsideRange) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() {
      SetSequenceParams(Persist(kTestAnimationSequence1),
                        Persist(kTestAnimationSequence2));
    }
    std::vector<MotionInfo> GetMotions() const override {
      return {
          Motion(
              kTestAnimationMotion1, TotalDurationMs(1000), gfx::Tween::LINEAR,
              Animate(kTestAnimationSequence1, FromValue(0.0), ToValue(1.0)),
              Animate(kTestAnimationSequence2, FromValue(1.0), ToValue(0.0))),
          Motion(kTestAnimationMotion2, TotalDurationMs(1000),
                 gfx::Tween::LINEAR,
                 Sequence(kTestAnimationSequence1,
                          Keyframe(AtPercent(0.25), Value(0.25)),
                          Keyframe(AtPercent(0.75), Value(0.75))),
                 Sequence(kTestAnimationSequence2,
                          Keyframe(AtPercent(0.25), Value(0.25)),
                          Keyframe(AtPercent(0.75), Value(0.75))))};
    }
  };
  SetProvider<P>();

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  FastForwardMs(1100);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion2);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
  FastForwardMs(500);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1),
                 0.5, 0.9);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2),
                 0.1, 0.5);
  FastForwardMs(600);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, Transition_CapAt_InsideRange) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() {
      SetSequenceParams(Persist(kTestAnimationSequence1),
                        Persist(kTestAnimationSequence2));
    }
    std::vector<MotionInfo> GetMotions() const override {
      return {
          Motion(kTestAnimationMotion1, TotalDurationMs(1000),
                 gfx::Tween::LINEAR,
                 Sequence(kTestAnimationSequence1, Transition::kCapAtOldValue,
                          Keyframe(AtPercent(0.0), Value(0.0)),
                          Keyframe(AtPercent(1.0), Value(1.0))),
                 Sequence(kTestAnimationSequence2, Transition::kCapAtOldValue,
                          Keyframe(AtPercent(0.0), Value(1.0)),
                          Keyframe(AtPercent(1.0), Value(0.0)))),
          Motion(kTestAnimationMotion2, TotalDurationMs(1000),
                 gfx::Tween::LINEAR,
                 Sequence(kTestAnimationSequence1, Transition::kCapAtOldValue,
                          Keyframe(AtPercent(0.25), Value(0.25)),
                          Keyframe(AtPercent(0.75), Value(0.75))),
                 Sequence(kTestAnimationSequence2, Transition::kCapAtOldValue,
                          Keyframe(AtPercent(0.25), Value(0.25)),
                          Keyframe(AtPercent(0.75), Value(0.75))))};
    }
  };
  SetProvider<P>();

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion2);
  FastForwardMs(1100);

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));
  FastForwardMs(100);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));
  FastForwardMs(500);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2),
                 0.25, 0.75);
  FastForwardMs(300);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1),
                 0.75, 1.0);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2),
                 0.0, 0.25);
  FastForwardMs(200);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, Transition_CapAt_OutsideRange) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() {
      SetSequenceParams(Persist(kTestAnimationSequence1),
                        Persist(kTestAnimationSequence2));
    }
    std::vector<MotionInfo> GetMotions() const override {
      return {
          Motion(kTestAnimationMotion1, TotalDurationMs(1000),
                 gfx::Tween::LINEAR,
                 Sequence(kTestAnimationSequence1, Transition::kCapAtOldValue,
                          Keyframe(AtPercent(0.0), Value(0.0)),
                          Keyframe(AtPercent(1.0), Value(1.0))),
                 Sequence(kTestAnimationSequence2, Transition::kCapAtOldValue,
                          Keyframe(AtPercent(0.0), Value(1.0)),
                          Keyframe(AtPercent(1.0), Value(0.0)))),
          Motion(kTestAnimationMotion2, TotalDurationMs(1000),
                 gfx::Tween::LINEAR,
                 Sequence(kTestAnimationSequence1, Transition::kCapAtOldValue,
                          Keyframe(AtPercent(0.25), Value(0.25)),
                          Keyframe(AtPercent(0.75), Value(0.75))),
                 Sequence(kTestAnimationSequence2, Transition::kCapAtOldValue,
                          Keyframe(AtPercent(0.25), Value(0.25)),
                          Keyframe(AtPercent(0.75), Value(0.75))))};
    }
  };
  SetProvider<P>();

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  FastForwardMs(1100);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion2);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
  FastForwardMs(500);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1),
                 0.5, 0.9);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2),
                 0.1, 0.5);
  FastForwardMs(600);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, Transition_Return) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() {
      SetSequenceParams(Persist(kTestAnimationSequence1),
                        Persist(kTestAnimationSequence2));
    }
    std::vector<MotionInfo> GetMotions() const override {
      return {Motion(kTestAnimationMotion1, TotalDurationMs(1000),
                     gfx::Tween::LINEAR,
                     Return(kTestAnimationSequence1, ToValue(1.0)),
                     Return(kTestAnimationSequence2, ToValue(0.0))),
              Motion(kTestAnimationMotion2, TotalDurationMs(1000),
                     gfx::Tween::LINEAR,
                     Sequence(kTestAnimationSequence1,
                              Keyframe(AtPercent(0.25), Value(0.25)),
                              Keyframe(AtPercent(0.75), Value(0.75))),
                     Sequence(kTestAnimationSequence2,
                              Keyframe(AtPercent(0.25), Value(0.25)),
                              Keyframe(AtPercent(0.75), Value(0.75))))};
    }
  };
  SetProvider<P>();

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion2);
  FastForwardMs(1100);

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));
  FastForwardMs(500);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1),
                 0.75, 1.0);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2),
                 0.0, 0.75);
  FastForwardMs(600);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, Transition_ImplicitReturn) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() {
      SetSequenceParams(Default(kTestAnimationSequence1, 1.0, true),
                        Default(kTestAnimationSequence2, 0.0, true));
    }
    std::vector<MotionInfo> GetMotions() const override {
      return {Motion(kTestAnimationMotion1, TotalDurationMs(1000),
                     gfx::Tween::LINEAR),
              Motion(kTestAnimationMotion2, TotalDurationMs(1000),
                     gfx::Tween::LINEAR,
                     Sequence(kTestAnimationSequence1,
                              Keyframe(AtPercent(0.25), Value(0.25)),
                              Keyframe(AtPercent(0.75), Value(0.75))),
                     Sequence(kTestAnimationSequence2,
                              Keyframe(AtPercent(0.25), Value(0.25)),
                              Keyframe(AtPercent(0.75), Value(0.75))))};
    }
  };
  SetProvider<P>();

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion2);
  FastForwardMs(1100);

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));
  FastForwardMs(500);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1),
                 0.75, 1.0);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2),
                 0.0, 0.75);
  FastForwardMs(600);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, Transition_NoImplicitReturn) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() {
      SetSequenceParams(Default(kTestAnimationSequence1, 1.0, true),
                        Default(kTestAnimationSequence2, 0.0, false),
                        Persist(kTestAnimationSequence3));
    }
    std::vector<MotionInfo> GetMotions() const override {
      return {
          Motion(kTestAnimationMotion1, TotalDurationMs(1000),
                 gfx::Tween::LINEAR),
          Motion(
              kTestAnimationMotion2, TotalDurationMs(1000), gfx::Tween::LINEAR,
              Sequence(kTestAnimationSequence1,
                       Keyframe(AtPercent(0.25), Value(0.25)),
                       Keyframe(AtPercent(0.75), Value(0.75))),
              Sequence(kTestAnimationSequence2,
                       Keyframe(AtPercent(0.25), Value(0.25)),
                       Keyframe(AtPercent(0.75), Value(0.75))),
              Animate(kTestAnimationSequence3, FromValue(0.0), ToValue(1.0)))};
    }
  };
  SetProvider<P>();

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion2);
  FastForwardMs(1100);

  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  FastForwardMs(1100);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence3));
}

TEST_F(BrowserAnimationControllerTest, GetCurrentValue) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() {
      SetSequenceParams(Default(kTestAnimationSequence1, 1.0, false),
                        Persist(kTestAnimationSequence2));
    }
    std::vector<MotionInfo> GetMotions() const override {
      return {Motion(kTestAnimationMotion2, TotalDurationMs(1000),
                     gfx::Tween::LINEAR,
                     Sequence(kTestAnimationSequence1,
                              Keyframe(AtPercent(0.25), Value(0.25)),
                              Keyframe(AtPercent(0.75), Value(0.75))),
                     Sequence(kTestAnimationSequence2,
                              Keyframe(AtPercent(0.25), Value(0.25)),
                              Keyframe(AtPercent(0.75), Value(0.75))))};
    }
  };
  SetProvider<P>();

  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(std::nullopt, controller()->GetCurrentValue(
                              kTestAnimationGroup, kTestAnimationSequence2));

  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion2);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));

  controller()->Clear(kTestAnimationGroup);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(std::nullopt, controller()->GetCurrentValue(
                              kTestAnimationGroup, kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, Transition_MidAnimation) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() {
      SetSequenceParams(Persist(kTestAnimationSequence1),
                        Persist(kTestAnimationSequence2));
    }
    std::vector<MotionInfo> GetMotions() const override {
      return {Motion(kTestAnimationMotion1, TotalDurationMs(1000),
                     gfx::Tween::LINEAR,
                     Return(kTestAnimationSequence1, ToValue(1.0)),
                     Return(kTestAnimationSequence2, ToValue(0.0))),
              Motion(kTestAnimationMotion2, TotalDurationMs(1000),
                     gfx::Tween::LINEAR,
                     Sequence(kTestAnimationSequence1,
                              Keyframe(AtPercent(0.25), Value(0.25)),
                              Keyframe(AtPercent(0.75), Value(0.75))),
                     Sequence(kTestAnimationSequence2,
                              Keyframe(AtPercent(0.25), Value(0.25)),
                              Keyframe(AtPercent(0.75), Value(0.75))))};
    }
  };
  SetProvider<P>();

  // Play half of an animation.
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion2);
  FastForwardMs(500);
  const double value1 = *controller()->GetCurrentValue(kTestAnimationGroup,
                                                       kTestAnimationSequence1);
  const double value2 = *controller()->GetCurrentValue(kTestAnimationGroup,
                                                       kTestAnimationSequence2);
  EXPECT_BETWEEN(value1, 0.35, 0.65);
  EXPECT_BETWEEN(value2, 0.35, 0.65);

  // Start the other animation without finishing the initial one.
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(value1, controller()->GetCurrentValue(kTestAnimationGroup,
                                                  kTestAnimationSequence1));
  EXPECT_EQ(value2, controller()->GetCurrentValue(kTestAnimationGroup,
                                                  kTestAnimationSequence2));
  FastForwardMs(500);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1),
                 value1, 1.0);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2),
                 0.0, value2);
  FastForwardMs(600);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
}

namespace {

class ResetAnimationProvider : public SingleGroupAnimationProvider {
 public:
  ResetAnimationProvider() {
    SetSequenceParams(Persist(kTestAnimationSequence1),
                      Persist(kTestAnimationSequence2));
  }
  std::vector<MotionInfo> GetMotions() const override {
    return {
        Motion(kTestAnimationMotion1, TotalDurationMs(1000), gfx::Tween::LINEAR,
               Animate(kTestAnimationSequence1, FromValue(0.0), ToValue(1.0)),
               Animate(kTestAnimationSequence2, FromValue(1.0), ToValue(0.0))),
        Motion(kTestAnimationMotion2, TotalDurationMs(1000), gfx::Tween::LINEAR,
               Sequence(kTestAnimationSequence1,
                        Keyframe(AtPercent(0.25), Value(0.25)),
                        Keyframe(AtPercent(0.75), Value(0.75))),
               Sequence(kTestAnimationSequence2,
                        Keyframe(AtPercent(0.25), Value(0.25)),
                        Keyframe(AtPercent(0.75), Value(0.75))))};
  }
};

}  // namespace

TEST_F(BrowserAnimationControllerTest, Reset_NoCurrentMotion) {
  SetProvider<ResetAnimationProvider>();
  UNCALLED_MOCK_CALLBACK(BrowserAnimationCallback, callback);
  const auto subscription =
      controller()->Subscribe(kTestAnimationGroup, callback.Get());
  EXPECT_CALL_IN_SCOPE(
      callback, Run(testing::_, BrowserAnimationUpdate::kEnded),
      controller()->Reset(kTestAnimationGroup, kTestAnimationMotion1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
  EXPECT_CALL_IN_SCOPE(
      callback, Run(testing::_, BrowserAnimationUpdate::kEnded),
      controller()->Reset(kTestAnimationGroup, kTestAnimationMotion2));
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, Reset_MidMotion_NoMotionSpecified) {
  SetProvider<ResetAnimationProvider>();
  UNCALLED_MOCK_CALLBACK(BrowserAnimationCallback, callback);
  const auto subscription =
      controller()->Subscribe(kTestAnimationGroup, callback.Get());

  EXPECT_CALL(callback, Run(testing::_, BrowserAnimationUpdate::kStarted));
  EXPECT_CALL(callback, Run(testing::_, BrowserAnimationUpdate::kProgressed))
      .Times(testing::AnyNumber());
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  FastForwardMs(500);
  EXPECT_CALL_IN_SCOPE(callback,
                       Run(testing::_, BrowserAnimationUpdate::kEnded),
                       controller()->Reset(kTestAnimationGroup));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, Reset_MidMotion_DifferentMotion) {
  SetProvider<ResetAnimationProvider>();
  UNCALLED_MOCK_CALLBACK(BrowserAnimationCallback, callback);
  const auto subscription =
      controller()->Subscribe(kTestAnimationGroup, callback.Get());

  EXPECT_CALL(callback, Run(testing::_, BrowserAnimationUpdate::kStarted));
  EXPECT_CALL(callback, Run(testing::_, BrowserAnimationUpdate::kProgressed))
      .Times(testing::AnyNumber());
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  FastForwardMs(500);

  testing::InSequence in_sequence;
  EXPECT_CALL(callback, Run(testing::_, BrowserAnimationUpdate::kCanceled))
      .WillOnce([](const BrowserAnimationController* controller,
                   BrowserAnimationUpdate) {
        EXPECT_EQ(kTestAnimationMotion1,
                  controller->GetCurrentMotion(kTestAnimationGroup));
      });
  EXPECT_CALL(callback, Run(testing::_, BrowserAnimationUpdate::kEnded))
      .WillOnce([](const BrowserAnimationController* controller,
                   BrowserAnimationUpdate) {
        EXPECT_EQ(kTestAnimationMotion2,
                  controller->GetCurrentMotion(kTestAnimationGroup));
      });
  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion2);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, Reset_MidMotion_SameMotion) {
  SetProvider<ResetAnimationProvider>();
  UNCALLED_MOCK_CALLBACK(BrowserAnimationCallback, callback);
  const auto subscription =
      controller()->Subscribe(kTestAnimationGroup, callback.Get());

  EXPECT_CALL(callback, Run(testing::_, BrowserAnimationUpdate::kStarted));
  EXPECT_CALL(callback, Run(testing::_, BrowserAnimationUpdate::kProgressed))
      .Times(testing::AnyNumber());
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  FastForwardMs(500);
  EXPECT_CALL_IN_SCOPE(
      callback, Run(testing::_, BrowserAnimationUpdate::kEnded),
      controller()->Reset(kTestAnimationGroup, kTestAnimationMotion1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, Reset_NoMotion_NoMotionSpecified) {
  SetProvider<ResetAnimationProvider>();
  UNCALLED_MOCK_CALLBACK(BrowserAnimationCallback, callback);
  const auto subscription =
      controller()->Subscribe(kTestAnimationGroup, callback.Get());
  controller()->Reset(kTestAnimationGroup);
}

namespace {

class DefaultsAnimationProvider : public SingleGroupAnimationProvider {
 public:
  DefaultsAnimationProvider() {
    SetSequenceParams(Default(kTestAnimationSequence1, 0.0, false),
                      Default(kTestAnimationSequence2, 0.0, false));
  }
  std::vector<MotionInfo> GetMotions() const override {
    return {
        Motion(kTestAnimationMotion1, TotalDurationMs(1000), gfx::Tween::LINEAR,
               Animate(kTestAnimationSequence1, FromValue(DefaultValue()),
                       ToValue(0.5)),
               Animate(kTestAnimationSequence2, FromValue(1.0),
                       ToValue(DefaultValue()))),
        Motion(kTestAnimationMotion2, TotalDurationMs(1000), gfx::Tween::LINEAR,
               Animate(kTestAnimationSequence1, FromValue(0.5),
                       ToValue(DefaultValue())),
               Animate(kTestAnimationSequence2, FromValue(DefaultValue()),
                       ToValue(1.0)))};
  }
};

}  // namespace

TEST_F(BrowserAnimationControllerTest, Defaults_UpdateSequenceParams) {
  auto* const provider = SetProvider<DefaultsAnimationProvider>();
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
  provider->UpdateSequenceParams(kTestAnimationGroup, kTestAnimationSequence1,
                                 std::nullopt);
  EXPECT_EQ(std::nullopt, controller()->GetCurrentValue(
                              kTestAnimationGroup, kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
  provider->UpdateSequenceParams(
      kTestAnimationGroup, kTestAnimationSequence1,
      internal::BrowserAnimationSequenceParams{
          .persist_between_animations = true, .default_value = 0.5});
  EXPECT_EQ(0.5, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
  provider->UpdateSequenceParams(
      kTestAnimationGroup, kTestAnimationSequence1,
      internal::BrowserAnimationSequenceParams{
          .persist_between_animations = false, .default_value = 0.5});
  EXPECT_EQ(std::nullopt, controller()->GetCurrentValue(
                              kTestAnimationGroup, kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest, Defaults_ResetPreservesDefaults) {
  auto* const provider = SetProvider<DefaultsAnimationProvider>();
  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(0.5, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
  provider->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence1,
                               0.9);
  provider->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence2,
                               0.9);
  EXPECT_EQ(0.5, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(0.9, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion2);
  EXPECT_EQ(0.9, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
  provider->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence1,
                               0.1);
  provider->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence2,
                               0.1);
  EXPECT_EQ(0.1, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2));
}

TEST_F(BrowserAnimationControllerTest,
       Defaults_ChangedDefaultRedirectsAnimation) {
  auto* const provider = SetProvider<DefaultsAnimationProvider>();
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  FastForwardMs(400);
  EXPECT_LT(controller()->GetCurrentValue(kTestAnimationGroup,
                                          kTestAnimationSequence1),
            0.5);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2),
                 0.2, 0.7);
  provider->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence1,
                               1.0);
  provider->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence2,
                               0.5);
  FastForwardMs(200);
  EXPECT_GT(controller()->GetCurrentValue(kTestAnimationGroup,
                                          kTestAnimationSequence1),
            0.5);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence2),
                 0.6, 0.9);
}

TEST_F(BrowserAnimationControllerTest,
       Defaults_ChangedDefaultBeforeAnimationAffectsTransition) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() {
      SetSequenceParams(Default(kTestAnimationSequence1, 0.0, false),
                        Default(kTestAnimationSequence2, 0.0, false));
    }
    std::vector<MotionInfo> GetMotions() const override {
      return {
          Motion(kTestAnimationMotion1, TotalDurationMs(1000),
                 gfx::Tween::LINEAR,
                 Animate(kTestAnimationSequence1, FromValue(1.0),
                         ToValue(DefaultValue()))),
          Motion(
              kTestAnimationMotion2, TotalDurationMs(1000), gfx::Tween::LINEAR,
              Animate(kTestAnimationSequence1, FromValue(0.0), ToValue(1.0)))};
    }
  };
  P* const p = SetProvider<P>();
  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion1);
  p->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence1, 0.5);
  EXPECT_EQ(0.5, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion2);
  EXPECT_EQ(0.5, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  FastForwardMs(500);
  EXPECT_BETWEEN(controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1),
                 0.6, 0.9);
}

TEST_F(BrowserAnimationControllerTest, Defaults_MaxOfDefaultAnd) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() { SetSequenceParams(Default(kTestAnimationSequence1, 0.0, true)); }
    std::vector<MotionInfo> GetMotions() const override {
      return {Motion(kTestAnimationMotion1,
                     Sequence(
                         kTestAnimationSequence1, StartingValue(DefaultValue()),
                         Segment(StartMs(0), EndMs(250),
                                 ToValue(MaxOfDefaultAnd(0.5))),
                         Segment(StartMs(750), EndMs(1000), ToValue(1.0)))),
              Motion(kTestAnimationMotion2, TotalDurationMs(100),
                     gfx::Tween::LINEAR)};
    }
  };
  P* const p = SetProvider<P>();
  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion2);
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  FastForwardMs(500);
  EXPECT_EQ(0.5, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  FastForwardMs(600);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));

  p->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence1, 0.25);
  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion2);
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(0.25, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  FastForwardMs(500);
  EXPECT_EQ(0.5, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  FastForwardMs(600);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));

  p->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence1, 0.75);
  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion2);
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  FastForwardMs(500);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  FastForwardMs(600);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));

  p->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence1, 1.0);
  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion2);
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  FastForwardMs(500);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  FastForwardMs(600);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
}

TEST_F(BrowserAnimationControllerTest,
       Defaults_MaxOfDefaultAnd_ActuallyUsesDefault) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() { SetSequenceParams(Default(kTestAnimationSequence1, 0.5, true)); }
    std::vector<MotionInfo> GetMotions() const override {
      return {Motion(kTestAnimationMotion1,
                     Sequence(
                         kTestAnimationSequence1, StartingValue(DefaultValue()),
                         Segment(StartMs(0), EndMs(250),
                                 ToValue(MaxOfDefaultAnd(0.5))),
                         Segment(StartMs(750), EndMs(1000), ToValue(1.0)))),
              Motion(kTestAnimationMotion2, TotalDurationMs(100),
                     gfx::Tween::LINEAR)};
    }
  };
  P* const p = SetProvider<P>();
  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion2);
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  FastForwardMs(500);
  EXPECT_EQ(0.5, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));

  p->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence1, 0.6);
  EXPECT_EQ(0.6, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
}

TEST_F(BrowserAnimationControllerTest, Defaults_MinOfDefaultAnd) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() { SetSequenceParams(Default(kTestAnimationSequence1, 1.0, true)); }
    std::vector<MotionInfo> GetMotions() const override {
      return {Motion(kTestAnimationMotion1,
                     Sequence(
                         kTestAnimationSequence1, StartingValue(DefaultValue()),
                         Segment(StartMs(0), EndMs(250),
                                 ToValue(MinOfDefaultAnd(0.5))),
                         Segment(StartMs(750), EndMs(1000), ToValue(0.0)))),
              Motion(kTestAnimationMotion2, TotalDurationMs(100),
                     gfx::Tween::LINEAR)};
    }
  };
  P* const p = SetProvider<P>();
  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion2);
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(1.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  FastForwardMs(500);
  EXPECT_EQ(0.5, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  FastForwardMs(600);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));

  p->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence1, 0.75);
  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion2);
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(0.75, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  FastForwardMs(500);
  EXPECT_EQ(0.5, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  FastForwardMs(600);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));

  p->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence1, 0.25);
  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion2);
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(0.25, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  FastForwardMs(500);
  EXPECT_EQ(0.25, controller()->GetCurrentValue(kTestAnimationGroup,
                                                kTestAnimationSequence1));
  FastForwardMs(600);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));

  p->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence1, 0.0);
  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion2);
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  FastForwardMs(500);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
  FastForwardMs(600);
  EXPECT_EQ(0.0, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
}

TEST_F(BrowserAnimationControllerTest,
       Defaults_MinOfDefaultAnd_ActuallyUsesDefault) {
  class P : public SingleGroupAnimationProvider {
   public:
    P() { SetSequenceParams(Default(kTestAnimationSequence1, 0.5, true)); }
    std::vector<MotionInfo> GetMotions() const override {
      return {Motion(kTestAnimationMotion1,
                     Sequence(
                         kTestAnimationSequence1, StartingValue(DefaultValue()),
                         Segment(StartMs(0), EndMs(250),
                                 ToValue(MinOfDefaultAnd(0.5))),
                         Segment(StartMs(750), EndMs(1000), ToValue(0.0)))),
              Motion(kTestAnimationMotion2, TotalDurationMs(100),
                     gfx::Tween::LINEAR)};
    }
  };
  P* const p = SetProvider<P>();
  controller()->Reset(kTestAnimationGroup, kTestAnimationMotion2);
  controller()->Start(kTestAnimationGroup, kTestAnimationMotion1);
  FastForwardMs(500);
  EXPECT_EQ(0.5, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));

  p->UpdateDefaultValue(kTestAnimationGroup, kTestAnimationSequence1, 0.4);
  EXPECT_EQ(0.4, controller()->GetCurrentValue(kTestAnimationGroup,
                                               kTestAnimationSequence1));
}
