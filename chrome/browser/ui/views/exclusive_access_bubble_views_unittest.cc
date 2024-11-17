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
#include "ui/views/accessibility/view_accessibility.h"

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
  ExclusiveAccessBubbleType type;
  UserGoal goal;
  Shortcut shortcut;
  bool enable_feature;
  bool has_download = false;
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
    ExclusiveAccessBubbleParams params{
        .type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION};
    bubble_view_ = std::make_unique<ExclusiveAccessBubbleViews>(
        browser_view(), params, base::NullCallback());
  }

  void TearDown() override {
    bubble_view_.reset();
    TestWithBrowserView::TearDown();
  }

  void UpdateExclusiveAccessBubbleType(ExclusiveAccessBubbleType type,
                                       bool has_download = false) {
    // When `has_download` is true, the existing bubble type is not updated.
    // In that case, set the type first, then signify the download bit.
    ExclusiveAccessBubbleParams params{.type = type, .force_update = true};
    bubble_view_->Update(params, base::NullCallback());
    if (has_download) {
      params.has_download = true;
      bubble_view_->Update(params, base::NullCallback());
    }
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

  SubtleNotificationView* GetSubtleNotificationView() {
    return static_cast<SubtleNotificationView*>(bubble_view_->GetView());
  }

 private:
  std::unique_ptr<ExclusiveAccessBubbleViews> bubble_view_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ExclusiveAccessBubbleViewsTest, UpdateViewContent) {
  const InstructionTextTestCase& test_case = GetParam();
  UpdateExclusiveAccessBubbleType(test_case.type, test_case.has_download);
  EXPECT_EQ(GetInstructionViewText(),
            CreateInstructionText(test_case.goal, test_case.shortcut));
}

TEST_P(ExclusiveAccessBubbleViewsTest,
       SubtleNotificationViewAccessibleProperties) {
  ui::AXNodeData data;
  GetSubtleNotificationView()->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.role, ax::mojom::Role::kAlert);
  EXPECT_EQ(GetSubtleNotificationView()->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kAlert);

  GetSubtleNotificationView()->UpdateContent(u"Sample |Accessible| Text");

  data = ui::AXNodeData();
  GetSubtleNotificationView()->GetViewAccessibility().GetAccessibleNodeData(
      &data);
  EXPECT_EQ(data.GetString16Attribute(ax::mojom::StringAttribute::kName),
            u"Sample Accessible Text");
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
