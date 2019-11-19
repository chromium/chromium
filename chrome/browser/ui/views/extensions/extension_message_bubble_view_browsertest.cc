// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_browsertest.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"

namespace {

// Checks that the |bubble| is showing. Uses |reference_bounds| to ensure it is
// in approximately the correct position.
void CheckBubbleAgainstReferenceBounds(views::BubbleDialogDelegateView* bubble,
                                       const gfx::Rect& reference_bounds) {
  ASSERT_TRUE(bubble);

  // Do a rough check that the bubble is in the right place.
  gfx::Rect bubble_bounds = bubble->GetWidget()->GetWindowBoundsInScreen();
  // It should be below the reference view, but not too far below.
  EXPECT_GE(bubble_bounds.y(), reference_bounds.y());
  // The arrow should be poking into the anchor.
  const int kShadowWidth = 1;
  EXPECT_LE(bubble_bounds.y(), reference_bounds.bottom() + kShadowWidth);
  // The bubble should intersect the reference view somewhere along the x-axis.
  EXPECT_FALSE(bubble_bounds.x() > reference_bounds.right());
  EXPECT_FALSE(reference_bounds.x() > bubble_bounds.right());

  // And, of course, the bubble should be visible...
  EXPECT_TRUE(bubble->GetVisible());
  // ... as should its Widget.
  EXPECT_TRUE(bubble->GetWidget()->IsVisible());
}

}  // namespace

class ExtensionMessageBubbleViewBrowserTest
    : public SupportsTestDialog<ExtensionMessageBubbleBrowserTest> {
 protected:
  ExtensionMessageBubbleViewBrowserTest() {}
  ~ExtensionMessageBubbleViewBrowserTest() override {}

  // TestBrowserDialog:
  void ShowUi(const std::string& name) override;

 private:
  // ExtensionMessageBubbleBrowserTest:
  void CheckBubbleNative(Browser* browser, AnchorPosition anchor) override;
  void CloseBubble(Browser* browser) override;
  void CloseBubbleNative(Browser* browser) override;
  void CheckBubbleIsNotPresentNative(Browser* browser) override;
  void ClickLearnMoreButton(Browser* browser) override;
  void ClickActionButton(Browser* browser) override;
  void ClickDismissButton(Browser* browser) override;

  // Whether to ignore requests from ExtensionMessageBubbleBrowserTest to
  // CloseBubble().
  bool block_close_ = false;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageBubbleViewBrowserTest);
};

void ExtensionMessageBubbleViewBrowserTest::ShowUi(const std::string& name) {
  // When invoked this way, the dialog test harness must close the bubble.
  base::AutoReset<bool> guard(&block_close_, true);

  if (name == "devmode_warning") {
    TestBubbleAnchoredToExtensionAction();
  } else if (name == "ntp_override") {
    TestControlledNewTabPageBubbleShown(false);
  } else {
    // TODO(tapted): Add cases for all bubble types.
    ADD_FAILURE() << "Unknown dialog: " << name;
  }
}

void ExtensionMessageBubbleViewBrowserTest::CheckBubbleNative(
    Browser* browser,
    AnchorPosition anchor) {
  gfx::Rect reference_bounds =
      GetAnchorReferenceBoundsForBrowser(browser, anchor);
  CheckBubbleAgainstReferenceBounds(GetViewsBubbleForBrowser(browser),
                                    reference_bounds);
}

void ExtensionMessageBubbleViewBrowserTest::CloseBubble(Browser* browser) {
  if (!block_close_)
    ExtensionMessageBubbleBrowserTest::CloseBubble(browser);
}

void ExtensionMessageBubbleViewBrowserTest::CloseBubbleNative(
    Browser* browser) {
  views::BubbleDialogDelegateView* bubble = GetViewsBubbleForBrowser(browser);
  ASSERT_TRUE(bubble);
  bubble->GetWidget()->Close();
  EXPECT_EQ(nullptr, GetViewsBubbleForBrowser(browser));
}

void ExtensionMessageBubbleViewBrowserTest::CheckBubbleIsNotPresentNative(
    Browser* browser) {
  EXPECT_EQ(nullptr, GetViewsBubbleForBrowser(browser));
}

void ExtensionMessageBubbleViewBrowserTest::ClickLearnMoreButton(
    Browser* browser) {
  ToolbarActionsBarBubbleViews* bubble = GetViewsBubbleForBrowser(browser);
  const views::ImageButton* learn_more = bubble->learn_more_button();
  const gfx::Point origin;
  static_cast<views::ButtonListener*>(bubble)->ButtonPressed(
      const_cast<views::ImageButton*>(learn_more),
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, origin, origin,
                     ui::EventTimeForNow(), 0, 0));

  // Clicking a button closes asynchronously. Since the close is asynchronous,
  // platform events may happen before the close completes and the dialog needs
  // to report a valid state.
  ui::AXNodeData node_data;
  bubble->GetWidget()->GetRootView()->GetAccessibleNodeData(&node_data);
  EXPECT_EQ(ax::mojom::Role::kAlertDialog, node_data.role);
}

void ExtensionMessageBubbleViewBrowserTest::ClickActionButton(
    Browser* browser) {
  ToolbarActionsBarBubbleViews* bubble = GetViewsBubbleForBrowser(browser);
  bubble->AcceptDialog();
}

void ExtensionMessageBubbleViewBrowserTest::ClickDismissButton(
    Browser* browser) {
  ToolbarActionsBarBubbleViews* bubble = GetViewsBubbleForBrowser(browser);
  bubble->CancelDialog();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleAnchoredToExtensionAction) {
  TestBubbleAnchoredToExtensionAction();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleAnchoredToAppMenu) {
  TestBubbleAnchoredToAppMenu();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleAnchoredToAppMenuWithOtherAction) {
  TestBubbleAnchoredToAppMenuWithOtherAction();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       PRE_ExtensionBubbleShowsOnStartup) {
  PreBubbleShowsOnStartup();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleShowsOnStartup) {
  TestBubbleShowsOnStartup();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestUninstallDangerousExtension) {
  TestUninstallDangerousExtension();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestDevModeBubbleIsntShownTwice) {
  TestDevModeBubbleIsntShownTwice();
}

// Tests for the extension bubble and settings overrides. These bubbles are
// currently only shown on Windows.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestControlledNewTabPageMessageBubble) {
  TestControlledNewTabPageBubbleShown(false);
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestControlledHomeMessageBubble) {
  TestControlledHomeBubbleShown();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestControlledSearchMessageBubble) {
  TestControlledSearchBubbleShown();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       PRE_TestControlledStartupMessageBubble) {
  PreTestControlledStartupBubbleShown();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestControlledStartupMessageBubble) {
  TestControlledStartupBubbleShown();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       PRE_TestControlledStartupNotShownOnRestart) {
  PreTestControlledStartupNotShownOnRestart();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestControlledStartupNotShownOnRestart) {
  TestControlledStartupNotShownOnRestart();
}

// BrowserUiTest for the warning bubble that appears when opening a new tab and
// an extension is controlling it. Only shown on Windows.
IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       InvokeUi_ntp_override) {
  ShowAndVerifyUi();
}

#endif  // defined(OS_WIN)

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestBubbleWithMultipleWindows) {
  TestBubbleWithMultipleWindows();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestClickingLearnMoreButton) {
  TestClickingLearnMoreButton();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestClickingActionButton) {
  TestClickingActionButton();
}

IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       TestClickingDismissButton) {
  TestClickingDismissButton();
}

// BrowserUiTest for the warning bubble that appears at startup when there are
// extensions installed in developer mode.
IN_PROC_BROWSER_TEST_F(ExtensionMessageBubbleViewBrowserTest,
                       InvokeUi_devmode_warning) {
  ShowAndVerifyUi();
}

class NtpExtensionBubbleViewBrowserTest
    : public ExtensionMessageBubbleViewBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionMessageBubbleViewBrowserTest::SetUpCommandLine(command_line);
// The NTP bubble is disabled by default on non-windows platforms.
#if !defined(OS_WIN)
    extensions::SetNtpBubbleEnabledForTesting(true);
#endif
  }

  void TearDownOnMainThread() override {
#if !defined(OS_WIN)
    extensions::SetNtpBubbleEnabledForTesting(false);
#endif
    ExtensionMessageBubbleViewBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(NtpExtensionBubbleViewBrowserTest,
                       TestControlledNewTabPageMessageBubbleLearnMore) {
  TestControlledNewTabPageBubbleShown(true);
}

// Flaky on Mac https://crbug.com/851655
#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_TestBubbleClosedAfterExtensionUninstall \
  DISABLED_TestBubbleClosedAfterExtensionUninstall
#else
#define MAYBE_TestBubbleClosedAfterExtensionUninstall \
  TestBubbleClosedAfterExtensionUninstall
#endif
IN_PROC_BROWSER_TEST_F(NtpExtensionBubbleViewBrowserTest,
                       MAYBE_TestBubbleClosedAfterExtensionUninstall) {
  TestBubbleClosedAfterExtensionUninstall();
}
