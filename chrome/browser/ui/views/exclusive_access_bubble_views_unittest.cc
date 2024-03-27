// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"

#include <string>

#include "build/buildflag.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "components/fullscreen_control/subtle_notification_view.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

enum UserGoal {
  kExitFullscreen = 0,
  kExitPointerLock = 1,
  kExitFullscreenAndSeeDownload = 2,
};

enum Shortcut {
  kPressEsc = 0,
  kPressAndHoldEsc = 1,
  kAccelerator = 2,
};

struct InstructionTextTestCase {
  std::string test_name;
  ExclusiveAccessBubbleType bubble_type;
  UserGoal goal;
  Shortcut shortcut;
  bool enable_feature;
  bool notify_download = false;
};

std::u16string GetUserGoalText(UserGoal goal) {
  switch (goal) {
    case kExitFullscreen:
      return u"To exit full screen";
    case kExitPointerLock:
      return u"To show your cursor";
    case kExitFullscreenAndSeeDownload:
      return u"To exit full screen and see download";
    default:
      return u"";
  }
}

std::u16string GetEscShortcutString(bool press_and_hold) {
  std::u16string esc = u"Esc";
#if BUILDFLAG(IS_MAC)
  esc = u"esc";
#endif

  return (press_and_hold ? u"press and hold |" : u"press |") + esc + u"|";
}

}  // namespace

class ExclusiveAccessBubbleViewsTest
    : public TestWithBrowserView,
      public testing::WithParamInterface<InstructionTextTestCase> {
 public:
  ExclusiveAccessBubbleViewsTest() {
    if (GetParam().enable_feature) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kPressAndHoldEscToExitBrowserFullscreen);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kPressAndHoldEscToExitBrowserFullscreen);
    }
  }

  void SetUp() override {
    TestWithBrowserView::SetUp();
    bubble_view_ = std::make_unique<ExclusiveAccessBubbleViews>(
        browser_view(), GURL(),
        EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
        /*notify_download=*/false, base::DoNothing());
  }

  void TearDown() override {
    bubble_view_.reset();
    TestWithBrowserView::TearDown();
  }

  void UpdateExclusiveAccessBubbleType(ExclusiveAccessBubbleType bubble_type,
                                       bool notify_download = false) {
    // When `notify_download` is True, `bubble_type` is preserved from the
    // old value, and not updated. So the test needs to set the `bubble_type`
    // by making another update without notifying download.
    if (notify_download) {
      bubble_view_->UpdateContent(GURL(), bubble_type, base::DoNothing(),
                                  /*notify_download=*/false,
                                  /*force_update=*/true);
    }
    bubble_view_->UpdateContent(GURL(), bubble_type, base::DoNothing(),
                                notify_download,
                                /*force_update=*/true);
  }

  std::u16string GetFullscreenAcceleratorString() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    return u"Fullscreen";
#else
    ui::Accelerator accelerator;
    chrome::AcceleratorProviderForBrowser(browser_view()->browser())
        ->GetAcceleratorForCommandId(IDC_FULLSCREEN, &accelerator);
    return accelerator.GetShortcutText();
#endif
  }

  std::u16string GetShortcutText(Shortcut shortcut) {
    switch (shortcut) {
      case kPressEsc:
        return GetEscShortcutString(false);
      case kPressAndHoldEsc:
        return GetEscShortcutString(true);
      case kAccelerator:
        return u"press |" + GetFullscreenAcceleratorString() + u"|";
      default:
        return u"";
    }
  }

  std::u16string CreateInstructionText(UserGoal goal, Shortcut shortcut) {
    return GetUserGoalText(goal) + u", " + GetShortcutText(shortcut);
  }

  std::u16string GetInstructionViewText() {
    return static_cast<SubtleNotificationView*>(bubble_view_->GetView())
        ->GetInstructionTextForTest();
  }

 private:
  std::unique_ptr<ExclusiveAccessBubbleViews> bubble_view_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ExclusiveAccessBubbleViewsTest, UpdateViewContent) {
  const InstructionTextTestCase& test_case = GetParam();
  UpdateExclusiveAccessBubbleType(
      test_case.bubble_type, /*notify_download=*/test_case.notify_download);
  EXPECT_EQ(GetInstructionViewText(),
            CreateInstructionText(test_case.goal, test_case.shortcut));
}

INSTANTIATE_TEST_SUITE_P(
    ExclusiveAccessTestInstantiation,
    ExclusiveAccessBubbleViewsTest,
    testing::ValuesIn<InstructionTextTestCase>({
        {"tabFullscreen",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreen, Shortcut::kPressEsc, false},
        {"tabFullscreenAndPointerLock",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_POINTERLOCK_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreen, Shortcut::kPressEsc, false},
        {"pointerLock",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_POINTERLOCK_EXIT_INSTRUCTION,
         UserGoal::kExitPointerLock, Shortcut::kPressEsc, false},
        {"tabFullscreenAndKeyboardLock",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreen, Shortcut::kPressAndHoldEsc, false},
        {"browserFullscreen",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreen, Shortcut::kAccelerator, false},
        {"extensionInitiatedBrowserFullscreen",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreen, Shortcut::kAccelerator, false},
        {"tabFullscreen_EnablePressAndHoldEsc",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreen, Shortcut::kPressEsc, true},
        {"tabFullscreenAndPointerLock_EnablePressAndHoldEsc",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_POINTERLOCK_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreen, Shortcut::kPressEsc, true},
        {"pointerLock_EnablePressAndHoldEsc",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_POINTERLOCK_EXIT_INSTRUCTION,
         UserGoal::kExitPointerLock, Shortcut::kPressEsc, true},
        {"tabFullscreenAndKeyboardLock_EnablePressAndHoldEsc",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreen, Shortcut::kPressAndHoldEsc, true},
        {"browserFullscreen_EnablePressAndHoldEsc",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreen, Shortcut::kPressAndHoldEsc, true},
        {"extensionInitiatedBrowserFullscreen_EnablePressAndHoldEsc",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreen, Shortcut::kPressAndHoldEsc, true},
        {"tabFullscreenSeeDownload",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreenAndSeeDownload, Shortcut::kPressEsc, false,
         true},
        {"browserFullscreenSeeDownload",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreenAndSeeDownload, Shortcut::kAccelerator, false,
         true},
        {"extensionInitiatedFullscreenSeeDownload",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreenAndSeeDownload, Shortcut::kAccelerator, false,
         true},
        {"tabFullscreenSeeDownload_EnablePressAndHoldEsc",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreenAndSeeDownload, Shortcut::kPressEsc, true,
         true},
        {"browserFullscreenSeeDownload_EnablePressAndHoldEsc",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreenAndSeeDownload, Shortcut::kPressAndHoldEsc,
         true, true},
        {"extensionInitiatedFullscreenSeeDownload_EnablePressAndHoldEsc",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreenAndSeeDownload, Shortcut::kPressAndHoldEsc,
         true, true},
    }),
    [](const testing::TestParamInfo<ExclusiveAccessBubbleViewsTest::ParamType>&
           info) { return info.param.test_name; });
