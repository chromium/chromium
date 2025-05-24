// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/exclusive_access_bubble_views.h"

#include <string>

#include "base/strings/string_util.h"
#include "build/buildflag.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_bubble_type.h"
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
  kSeeDownload = 3,
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
  bool enable_feature = false;
};

std::u16string GetUserGoalText(UserGoal goal) {
  switch (goal) {
    case kExitFullscreen:
      return u"To exit full screen";
    case kExitPointerLock:
      return u"To show your cursor";
    case kExitFullscreenAndSeeDownload:
      return u"To exit full screen and see download";
    case kSeeDownload:
      return u"Download started. To see it";
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

class ExclusiveAccessBubbleViewsTest : public TestWithBrowserView {
 public:
  ExclusiveAccessBubbleViewsTest() = default;

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
                                       bool has_download) {
    ExclusiveAccessBubbleParams params{
        .type = type, .has_download = has_download, .force_update = true};
    bubble_view_->Update(params, base::NullCallback());
  }

  std::u16string GetFullscreenAcceleratorString() {
#if BUILDFLAG(IS_CHROMEOS)
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

 protected:
  std::unique_ptr<ExclusiveAccessBubbleViews> bubble_view_;
};

class ExclusiveAccessBubbleViewsInstructionTextTest
    : public ExclusiveAccessBubbleViewsTest,
      public testing::WithParamInterface<InstructionTextTestCase> {
 public:
  ExclusiveAccessBubbleViewsInstructionTextTest() {
    if (GetParam().enable_feature) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kPressAndHoldEscToExitBrowserFullscreen);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kPressAndHoldEscToExitBrowserFullscreen);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(ExclusiveAccessBubbleViewsInstructionTextTest, UpdateViewContent) {
  const InstructionTextTestCase& test_case = GetParam();
  UpdateExclusiveAccessBubbleType(test_case.type, /*has_download=*/false);
  EXPECT_EQ(GetInstructionViewText(),
            CreateInstructionText(test_case.goal, test_case.shortcut));
}

TEST_F(ExclusiveAccessBubbleViewsTest,
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

// Tests where the bubble is updated once with the specified type.
INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    ExclusiveAccessBubbleViewsInstructionTextTest,
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
    }),
    [](const testing::TestParamInfo<
        ExclusiveAccessBubbleViewsInstructionTextTest::ParamType>& info) {
      return info.param.test_name;
    });

// Tests the creation of a has_download, non-overriding notification.
TEST_F(ExclusiveAccessBubbleViewsTest, CreateForDownload) {
  ExclusiveAccessBubbleParams params{.type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
                                     .has_download = true};
  bubble_view_ = std::make_unique<ExclusiveAccessBubbleViews>(
      browser_view(), params, base::NullCallback());
  EXPECT_TRUE(base::StartsWith(
      GetInstructionViewText(),
      CreateInstructionText(UserGoal::kSeeDownload, Shortcut::kPressEsc)));
}

// Tests the updating of a has_download, non-overriding notification with a
// second one of the same.
TEST_F(ExclusiveAccessBubbleViewsTest, CreateForDownloadUpdateForDownload) {
  ExclusiveAccessBubbleParams params{.type = EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
                                     .has_download = true};
  bubble_view_ = std::make_unique<ExclusiveAccessBubbleViews>(
      browser_view(), params, base::NullCallback());
  UpdateExclusiveAccessBubbleType(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
                                  /*has_download=*/true);
  EXPECT_TRUE(base::StartsWith(
      GetInstructionViewText(),
      CreateInstructionText(UserGoal::kSeeDownload, Shortcut::kPressEsc)));
}

// Tests updating the bubble first with the type, then a second time for a
// download notification.
using ExclusiveAccessBubbleViewsTestWithDownload =
    ExclusiveAccessBubbleViewsInstructionTextTest;
TEST_P(ExclusiveAccessBubbleViewsTestWithDownload,
       UpdateViewContentThenDownload) {
  const InstructionTextTestCase& test_case = GetParam();
  UpdateExclusiveAccessBubbleType(test_case.type, /*has_download=*/false);
  // Download notifications pass the type NONE.
  UpdateExclusiveAccessBubbleType(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
                                  /*has_download=*/true);
  EXPECT_EQ(GetInstructionViewText(),
            CreateInstructionText(test_case.goal, test_case.shortcut));
}

// Tests the above but with the Update() calls coming in the other order.
TEST_P(ExclusiveAccessBubbleViewsTestWithDownload,
       UpdateViewContentAfterDownload) {
  const InstructionTextTestCase& test_case = GetParam();
  // Update it for a download.
  UpdateExclusiveAccessBubbleType(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE,
                                  /*has_download=*/true);
  // A subsequent Update() call with the test-specified type.
  UpdateExclusiveAccessBubbleType(test_case.type, /*has_download=*/false);
  EXPECT_EQ(GetInstructionViewText(),
            CreateInstructionText(test_case.goal, test_case.shortcut));
}

INSTANTIATE_TEST_SUITE_P(
    /* no label */,
    ExclusiveAccessBubbleViewsTestWithDownload,
    testing::ValuesIn<InstructionTextTestCase>({
        {"tabFullscreenSeeDownload",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreenAndSeeDownload, Shortcut::kPressEsc, false},
        {"browserFullscreenSeeDownload",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreenAndSeeDownload, Shortcut::kAccelerator,
         false},
        {"extensionInitiatedFullscreenSeeDownload",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreenAndSeeDownload, Shortcut::kAccelerator,
         false},
        {"tabFullscreenSeeDownload_EnablePressAndHoldEsc",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreenAndSeeDownload, Shortcut::kPressEsc, true},
        {"browserFullscreenSeeDownload_EnablePressAndHoldEsc",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_BROWSER_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreenAndSeeDownload, Shortcut::kPressAndHoldEsc,
         true},
        {"extensionInitiatedFullscreenSeeDownload_EnablePressAndHoldEsc",
         EXCLUSIVE_ACCESS_BUBBLE_TYPE_EXTENSION_FULLSCREEN_EXIT_INSTRUCTION,
         UserGoal::kExitFullscreenAndSeeDownload, Shortcut::kPressAndHoldEsc,
         true},
    }),
    [](const testing::TestParamInfo<
        ExclusiveAccessBubbleViewsTestWithDownload::ParamType>& info) {
      return info.param.test_name;
    });
