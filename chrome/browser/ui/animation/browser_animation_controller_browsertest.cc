// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/animation/browser_animation_controller.h"

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/ui/animation/browser_animation_provider.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/identifier/unique_identifier.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/view.h"

namespace {

DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationGroup, kTestGroup);
DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationMotion, kTestMotion1);
DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationMotion, kTestMotion2);
DEFINE_LOCAL_UNIQUE_IDENTIFIER_VALUE(BrowserAnimationSequence, kTestSequence);

constexpr char kTestGroupName[] = "TestAnimationGroup";
constexpr char kTestMotion1Name[] = "Motion1";
constexpr char kTestMotion2Name[] = "Motion2";

class TestAnimationProvider : public CachingBrowserAnimationProvider {
 public:
  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  explicit TestAnimationProvider(bool register_group_histogram = true,
                                 bool register_motion_histogram = true) {
    SetSequenceParams(kTestGroup, Default(kTestSequence, 0.0, false));
    if (register_group_histogram) {
      SetHistogramName(kTestGroup, kTestGroupName);
    }
    if (register_motion_histogram) {
      SetHistogramName(kTestMotion1, kTestMotion1Name);
      SetHistogramName(kTestMotion2, kTestMotion2Name);
    }
  }
  ~TestAnimationProvider() override = default;

  GroupInfos GenerateAnimations() const override {
    return {
        Group(kTestGroup,
              Motion(kTestMotion1, TotalDurationMs(500), gfx::Tween::LINEAR,
                     Animate(kTestSequence, FromValue(0.0), ToValue(1.0))),
              Motion(kTestMotion2, TotalDurationMs(500), gfx::Tween::LINEAR,
                     Animate(kTestSequence, FromValue(1.0), ToValue(0.0))))};
  }
};

DEFINE_FRAMEWORK_SPECIFIC_METADATA(TestAnimationProvider)

class TestAnimationView : public views::View {
  METADATA_HEADER(TestAnimationView, views::View)
 public:
  explicit TestAnimationView(BrowserAnimationController* controller)
      : subscription_(controller->Subscribe(
            kTestGroup,
            base::BindRepeating(&TestAnimationView::OnAnimation,
                                base::Unretained(this)))) {}

 private:
  void OnAnimation(const BrowserAnimationController* controller,
                   BrowserAnimationUpdate update) {
    const double value =
        *controller->GetCurrentValue(kTestGroup, kTestSequence);
    const int size = base::ClampRound(100 + 100 * value);
    // This will force a layout and paint.
    SetPreferredSize(gfx::Size(size, size));
    parent()->SchedulePaint();
  }

  base::CallbackListSubscription subscription_;
};

BEGIN_METADATA(TestAnimationView)
END_METADATA

}  // namespace

class BrowserAnimationControllerBrowsertest : public InProcessBrowserTest {
 public:
  BrowserAnimationControllerBrowsertest() = default;
  ~BrowserAnimationControllerBrowsertest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    browser()->GetBrowserView().AddChildView(
        std::make_unique<TestAnimationView>(controller()));
    subscription_ = controller()->Subscribe(
        kTestGroup,
        base::BindRepeating(&BrowserAnimationControllerBrowsertest::OnAnimation,
                            base::Unretained(this)));
  }

 protected:
  static constexpr char kGroupOverride[] = "GroupOverride";
  static constexpr char kMotionOverride[] = "MotionOverride";

  const std::string kMotion1FPS = base::StringPrintf(
      "%s.%s%s",
      kTestGroupName,
      kTestMotion1Name,
      BrowserAnimationController::kFramesPerSecondHistogramSuffix);
  const std::string kMotion2FPS = base::StringPrintf(
      "%s.%s%s",
      kTestGroupName,
      kTestMotion2Name,
      BrowserAnimationController::kFramesPerSecondHistogramSuffix);
  const std::string kMotion1LongestFrame = base::StringPrintf(
      "%s.%s%s",
      kTestGroupName,
      kTestMotion1Name,
      BrowserAnimationController::kLongestFrameHistogramSuffix);
  const std::string kMotion2LongestFrame = base::StringPrintf(
      "%s.%s%s",
      kTestGroupName,
      kTestMotion2Name,
      BrowserAnimationController::kLongestFrameHistogramSuffix);

  void StartAnimation(
      BrowserAnimationMotion motion,
      std::optional<std::string_view> group_name = std::nullopt,
      std::optional<std::string_view> motion_name = std::nullopt) {
    waiting_ = true;
    controller()->Start(kTestGroup, motion, group_name, motion_name);
  }

  void FinishAnimation() {
    if (waiting_) {
      run_loop_.emplace(base::RunLoop::Type::kNestableTasksAllowed);
      run_loop_->Run();
    }
  }

  BrowserAnimationController* controller() {
    return BrowserAnimationController::From(browser());
  }
  base::HistogramTester* histogram_tester() { return &histogram_tester_; }

 private:
  void OnAnimation(const BrowserAnimationController* controller,
                   BrowserAnimationUpdate update) {
    if (update == BrowserAnimationUpdate::kEnded) {
      waiting_ = false;
      if (run_loop_) {
        run_loop_->Quit();
      }
    }
  }

  std::optional<base::RunLoop> run_loop_;
  bool waiting_ = false;
  base::HistogramTester histogram_tester_;
  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(BrowserAnimationControllerBrowsertest, EmitsHistograms) {
  controller()->AddAnimationProvider(std::make_unique<TestAnimationProvider>());

  histogram_tester()->ExpectTotalCount(kMotion1FPS, 0);
  histogram_tester()->ExpectTotalCount(kMotion2FPS, 0);
  histogram_tester()->ExpectTotalCount(kMotion1LongestFrame, 0);
  histogram_tester()->ExpectTotalCount(kMotion2LongestFrame, 0);

  StartAnimation(kTestMotion1);
  histogram_tester()->ExpectTotalCount(kMotion1FPS, 0);
  histogram_tester()->ExpectTotalCount(kMotion2FPS, 0);
  histogram_tester()->ExpectTotalCount(kMotion1LongestFrame, 0);
  histogram_tester()->ExpectTotalCount(kMotion2LongestFrame, 0);

  FinishAnimation();
  histogram_tester()->ExpectTotalCount(kMotion1FPS, 1);
  histogram_tester()->ExpectTotalCount(kMotion2FPS, 0);
  histogram_tester()->ExpectTotalCount(kMotion1LongestFrame, 1);
  histogram_tester()->ExpectTotalCount(kMotion2LongestFrame, 0);

  auto samples = histogram_tester()->GetAllSamples(kMotion1FPS);
  EXPECT_GE(samples[0].min, 0);
  samples = histogram_tester()->GetAllSamples(kMotion1LongestFrame);
  EXPECT_GE(samples[0].min, 0);

  StartAnimation(kTestMotion2);
  FinishAnimation();
  histogram_tester()->ExpectTotalCount(kMotion1FPS, 1);
  histogram_tester()->ExpectTotalCount(kMotion2FPS, 1);
  histogram_tester()->ExpectTotalCount(kMotion1LongestFrame, 1);
  histogram_tester()->ExpectTotalCount(kMotion2LongestFrame, 1);

  histogram_tester()->GetAllSamples(kMotion2FPS);
  EXPECT_GE(samples[0].min, 0);
  samples = histogram_tester()->GetAllSamples(kMotion2LongestFrame);
  EXPECT_GE(samples[0].min, 0);
}

IN_PROC_BROWSER_TEST_F(BrowserAnimationControllerBrowsertest,
                       EmitsHistogram_AfterRedirect) {
  controller()->AddAnimationProvider(std::make_unique<TestAnimationProvider>());

  StartAnimation(kTestMotion1);
  StartAnimation(kTestMotion2);
  FinishAnimation();
  histogram_tester()->ExpectTotalCount(kMotion1FPS, 0);
  histogram_tester()->ExpectTotalCount(kMotion2FPS, 1);
  histogram_tester()->ExpectTotalCount(kMotion1LongestFrame, 0);
  histogram_tester()->ExpectTotalCount(kMotion2LongestFrame, 1);

  auto samples = histogram_tester()->GetAllSamples(kMotion2FPS);
  EXPECT_GE(samples[0].min, 0);
  samples = histogram_tester()->GetAllSamples(kMotion2LongestFrame);
  EXPECT_GE(samples[0].min, 0);
}

IN_PROC_BROWSER_TEST_F(BrowserAnimationControllerBrowsertest,
                       DoesNotEmitHistograms_NoGroup) {
  controller()->AddAnimationProvider(
      std::make_unique<TestAnimationProvider>(false, true));

  StartAnimation(kTestMotion1);
  FinishAnimation();
  histogram_tester()->ExpectTotalCount(kMotion1FPS, 0);
  histogram_tester()->ExpectTotalCount(kMotion2FPS, 0);
  histogram_tester()->ExpectTotalCount(kMotion1LongestFrame, 0);
  histogram_tester()->ExpectTotalCount(kMotion2LongestFrame, 0);
}

IN_PROC_BROWSER_TEST_F(BrowserAnimationControllerBrowsertest,
                       DoesNotEmitHistograms_NoMotion) {
  controller()->AddAnimationProvider(
      std::make_unique<TestAnimationProvider>(true, false));

  StartAnimation(kTestMotion1);
  FinishAnimation();
  histogram_tester()->ExpectTotalCount(kMotion1FPS, 0);
  histogram_tester()->ExpectTotalCount(kMotion2FPS, 0);
  histogram_tester()->ExpectTotalCount(kMotion1LongestFrame, 0);
  histogram_tester()->ExpectTotalCount(kMotion2LongestFrame, 0);
}

IN_PROC_BROWSER_TEST_F(BrowserAnimationControllerBrowsertest,
                       DoesNotEmitHistograms_Canceled) {
  controller()->AddAnimationProvider(
      std::make_unique<TestAnimationProvider>(true, false));

  StartAnimation(kTestMotion1);
  controller()->Reset(kTestGroup);
  histogram_tester()->ExpectTotalCount(kMotion1FPS, 0);
  histogram_tester()->ExpectTotalCount(kMotion2FPS, 0);
  histogram_tester()->ExpectTotalCount(kMotion1LongestFrame, 0);
  histogram_tester()->ExpectTotalCount(kMotion2LongestFrame, 0);
}

IN_PROC_BROWSER_TEST_F(BrowserAnimationControllerBrowsertest,
                       DoesNotEmitHistograms_NoGroupOrMotion) {
  controller()->AddAnimationProvider(
      std::make_unique<TestAnimationProvider>(false, false));

  StartAnimation(kTestMotion1);
  FinishAnimation();
  histogram_tester()->ExpectTotalCount(kMotion1FPS, 0);
  histogram_tester()->ExpectTotalCount(kMotion2FPS, 0);
  histogram_tester()->ExpectTotalCount(kMotion1LongestFrame, 0);
  histogram_tester()->ExpectTotalCount(kMotion2LongestFrame, 0);
}

IN_PROC_BROWSER_TEST_F(BrowserAnimationControllerBrowsertest,
                       EmitsHistogram_OverrideGroup_ReplacesExisting) {
  controller()->AddAnimationProvider(
      std::make_unique<TestAnimationProvider>(true, true));
  const auto expected = base::StringPrintf(
      "%s.%s%s", kGroupOverride, kTestMotion1Name,
      BrowserAnimationController::kFramesPerSecondHistogramSuffix);

  StartAnimation(kTestMotion1, kGroupOverride);
  FinishAnimation();
  histogram_tester()->ExpectTotalCount(expected, 1);
}

IN_PROC_BROWSER_TEST_F(BrowserAnimationControllerBrowsertest,
                       EmitsHistogram_OverrideGroup_ReplacesNull) {
  controller()->AddAnimationProvider(
      std::make_unique<TestAnimationProvider>(false, true));
  const auto expected = base::StringPrintf(
      "%s.%s%s", kGroupOverride, kTestMotion1Name,
      BrowserAnimationController::kFramesPerSecondHistogramSuffix);

  StartAnimation(kTestMotion1, kGroupOverride);
  FinishAnimation();
  histogram_tester()->ExpectTotalCount(expected, 1);
}

IN_PROC_BROWSER_TEST_F(BrowserAnimationControllerBrowsertest,
                       EmitsHistogram_OverrideMotion_ReplacesExisting) {
  controller()->AddAnimationProvider(
      std::make_unique<TestAnimationProvider>(true, true));
  const auto expected = base::StringPrintf(
      "%s.%s%s", kTestGroupName, kMotionOverride,
      BrowserAnimationController::kFramesPerSecondHistogramSuffix);

  StartAnimation(kTestMotion1, std::nullopt, kMotionOverride);
  FinishAnimation();
  histogram_tester()->ExpectTotalCount(expected, 1);
}

IN_PROC_BROWSER_TEST_F(BrowserAnimationControllerBrowsertest,
                       EmitsHistogram_OverrideMotion_ReplacesNull) {
  controller()->AddAnimationProvider(
      std::make_unique<TestAnimationProvider>(true, false));
  const auto expected = base::StringPrintf(
      "%s.%s%s", kTestGroupName, kMotionOverride,
      BrowserAnimationController::kFramesPerSecondHistogramSuffix);

  StartAnimation(kTestMotion1, std::nullopt, kMotionOverride);
  FinishAnimation();
  histogram_tester()->ExpectTotalCount(expected, 1);
}

IN_PROC_BROWSER_TEST_F(BrowserAnimationControllerBrowsertest,
                       EmitsHistogram_NoGroup) {
  controller()->AddAnimationProvider(
      std::make_unique<TestAnimationProvider>(true, true));
  const auto expected = base::StringPrintf(
      "%s%s", kTestMotion1Name,
      BrowserAnimationController::kFramesPerSecondHistogramSuffix);

  StartAnimation(kTestMotion1, "");
  FinishAnimation();
  histogram_tester()->ExpectTotalCount(expected, 1);
}

IN_PROC_BROWSER_TEST_F(BrowserAnimationControllerBrowsertest,
                       EmitsHistogram_NoMotion) {
  controller()->AddAnimationProvider(
      std::make_unique<TestAnimationProvider>(true, true));
  const auto expected = base::StringPrintf(
      "%s%s", kTestGroupName,
      BrowserAnimationController::kFramesPerSecondHistogramSuffix);

  StartAnimation(kTestMotion1, std::nullopt, "");
  FinishAnimation();
  histogram_tester()->ExpectTotalCount(expected, 1);
}
