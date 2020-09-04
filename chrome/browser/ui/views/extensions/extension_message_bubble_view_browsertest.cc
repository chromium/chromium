// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/extensions/extension_message_bubble_browsertest.h"
#include "chrome/browser/ui/extensions/settings_api_bubble_helpers.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/toolbar/toolbar_actions_bar.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_actions_bar_bubble_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "content/public/test/browser_test.h"
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

// Returns the bubble that is currently attached to |browser|, or null if there
// is no bubble showing.
ToolbarActionsBarBubbleViews* GetViewsBubbleForBrowser(Browser* browser) {
  return static_cast<ToolbarActionsBarBubbleViews*>(
      BrowserView::GetBrowserViewForBrowser(browser)
          ->toolbar_button_provider()
          ->GetBrowserActionsContainer()
          ->active_bubble());
}

// Returns the expected test anchor bounds on |browser|.
gfx::Rect GetAnchorReferenceBoundsForBrowser(
    Browser* browser,
    ExtensionMessageBubbleBrowserTest::AnchorPosition anchor) {
  auto* const toolbar_button_provider =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar_button_provider();
  auto* const browser_actions_container =
      toolbar_button_provider->GetBrowserActionsContainer();
  views::View* anchor_view = nullptr;
  switch (anchor) {
    case ExtensionMessageBubbleBrowserTest::ANCHOR_BROWSER_ACTION:
      EXPECT_GT(browser_actions_container->num_toolbar_actions(), 0u);
      if (browser_actions_container->num_toolbar_actions() == 0)
        return gfx::Rect();
      anchor_view = browser_actions_container->GetToolbarActionViewAt(0);
      break;
    case ExtensionMessageBubbleBrowserTest::ANCHOR_APP_MENU:
      anchor_view = toolbar_button_provider->GetAppMenuButton();
      break;
  }

  EXPECT_TRUE(anchor_view);
  EXPECT_EQ(anchor_view,
            browser_actions_container->active_bubble()->GetAnchorView());
  return anchor_view->GetBoundsInScreen();
}

}  // namespace

class ExtensionMessageBubbleViewBrowserTest
    : public SupportsTestDialog<ExtensionMessageBubbleBrowserTest> {
 protected:
  ExtensionMessageBubbleViewBrowserTest() {}
  ~ExtensionMessageBubbleViewBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override;

  // TestBrowserDialog:
  void ShowUi(const std::string& name) override;

  // Returns a list of features to disable.
  virtual std::vector<base::Feature> GetFeaturesToDisable();

 private:
  // ExtensionMessageBubbleBrowserTest:
  void CheckBubbleNative(Browser* browser, AnchorPosition anchor) override;
  void CloseBubble(Browser* browser) override;
  void CloseBubbleNative(Browser* browser) override;
  void CheckBubbleIsNotPresentNative(Browser* browser) override;
  void ClickLearnMoreButton(Browser* browser) override;
  void ClickActionButton(Browser* browser) override;
  void ClickDismissButton(Browser* browser) override;

  base::test::ScopedFeatureList feature_list_;

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

void ExtensionMessageBubbleViewBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Note: The ScopedFeatureList needs to be instantiated before the rest of
  // set up happens.
  feature_list_.InitWithFeatures({}, GetFeaturesToDisable());

  ExtensionMessageBubbleBrowserTest::SetUpCommandLine(command_line);
}

std::vector<base::Feature>
ExtensionMessageBubbleViewBrowserTest::GetFeaturesToDisable() {
  return {};
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

// A test suite that runs with the old toolbar UI, instead of with the
// new Extensions Menu.
// TODO(devlin): Isolate out the tests that fundamentally rely on the old UI
// from the ones that can run with the extensions menu.
// https://crbug.com/1100412.
class LegacyExtensionMessageBubbleViewBrowserTest
    : public ExtensionMessageBubbleViewBrowserTest {
 public:
  LegacyExtensionMessageBubbleViewBrowserTest() = default;
  ~LegacyExtensionMessageBubbleViewBrowserTest() override = default;

  std::vector<base::Feature> GetFeaturesToDisable() override {
    return {features::kExtensionsToolbarMenu};
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ExtensionMessageBubbleViewBrowserTest::SetUpCommandLine(command_line);
    ToolbarActionsBar::set_extension_bubble_appearance_wait_time_for_testing(0);
    ToolbarActionsBar::disable_animations_for_testing_ = true;
  }

  void TearDownOnMainThread() override {
    ToolbarActionsBar::disable_animations_for_testing_ = false;
    ExtensionMessageBubbleViewBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleAnchoredToExtensionAction) {
  TestBubbleAnchoredToExtensionAction();
}

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleAnchoredToAppMenu) {
  TestBubbleAnchoredToAppMenu();
}

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleAnchoredToAppMenuWithOtherAction) {
  TestBubbleAnchoredToAppMenuWithOtherAction();
}

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       PRE_ExtensionBubbleShowsOnStartup) {
  PreBubbleShowsOnStartup();
}

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       ExtensionBubbleShowsOnStartup) {
  TestBubbleShowsOnStartup();
}

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       TestUninstallDangerousExtension) {
  TestUninstallDangerousExtension();
}

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       TestDevModeBubbleIsntShownTwice) {
  TestDevModeBubbleIsntShownTwice();
}

// Tests for the extension bubble and settings overrides. These bubbles are
// currently only shown on Windows.
// TODO(devlin): No they're not. We should enable all of these on Mac.
#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       TestControlledHomeMessageBubble) {
  TestControlledHomeBubbleShown();
}

class ControlledSearchMessageBubbleViewBrowserTest
    : public LegacyExtensionMessageBubbleViewBrowserTest {
 public:
  ControlledSearchMessageBubbleViewBrowserTest() = default;
  ~ControlledSearchMessageBubbleViewBrowserTest() override = default;

  std::vector<base::Feature> GetFeaturesToDisable() override {
    std::vector<base::Feature> features_to_disable =
        LegacyExtensionMessageBubbleViewBrowserTest::GetFeaturesToDisable();
    // The kExtensionSettingsOverriddenDialogs introduces a new UI for the
    // controlled search confirmation. Disable it to test the old UI.
    features_to_disable.push_back(
        features::kExtensionSettingsOverriddenDialogs);
    return features_to_disable;
  }
};

IN_PROC_BROWSER_TEST_F(ControlledSearchMessageBubbleViewBrowserTest,
                       TestControlledSearchMessageBubble) {
  TestControlledSearchBubbleShown();
}

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       PRE_TestControlledStartupMessageBubble) {
  PreTestControlledStartupBubbleShown();
}

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       TestControlledStartupMessageBubble) {
  TestControlledStartupBubbleShown();
}

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       PRE_TestControlledStartupNotShownOnRestart) {
  PreTestControlledStartupNotShownOnRestart();
}

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       TestControlledStartupNotShownOnRestart) {
  TestControlledStartupNotShownOnRestart();
}

#endif  // defined(OS_WIN)

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       TestBubbleWithMultipleWindows) {
  TestBubbleWithMultipleWindows();
}

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       TestClickingLearnMoreButton) {
  TestClickingLearnMoreButton();
}

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       TestClickingActionButton) {
  TestClickingActionButton();
}

IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       TestClickingDismissButton) {
  TestClickingDismissButton();
}

// BrowserUiTest for the warning bubble that appears at startup when there are
// extensions installed in developer mode.
IN_PROC_BROWSER_TEST_F(LegacyExtensionMessageBubbleViewBrowserTest,
                       InvokeUi_devmode_warning) {
  ShowAndVerifyUi();
}

class NtpExtensionBubbleViewBrowserTest
    : public LegacyExtensionMessageBubbleViewBrowserTest {
 public:
  std::vector<base::Feature> GetFeaturesToDisable() override {
    std::vector<base::Feature> features_to_disable =
        LegacyExtensionMessageBubbleViewBrowserTest::GetFeaturesToDisable();
    features_to_disable.push_back(
        features::kExtensionSettingsOverriddenDialogs);
    return features_to_disable;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LegacyExtensionMessageBubbleViewBrowserTest::SetUpCommandLine(command_line);
// The NTP bubble is only enabled by default on Mac, Windows, and CrOS.
#if !defined(OS_WIN) && !defined(OS_MAC) && !defined(OS_CHROMEOS)
    extensions::SetNtpPostInstallUiEnabledForTesting(true);
#endif
  }

  void TearDownOnMainThread() override {
#if !defined(OS_WIN) && !defined(OS_MAC) && !defined(OS_CHROMEOS)
    extensions::SetNtpPostInstallUiEnabledForTesting(false);
#endif
    LegacyExtensionMessageBubbleViewBrowserTest::TearDownOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(NtpExtensionBubbleViewBrowserTest,
                       TestControlledNewTabPageMessageBubbleLearnMore) {
  TestControlledNewTabPageBubbleShown(true);
}

// Flaky on Mac https://crbug.com/851655
#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS)
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

IN_PROC_BROWSER_TEST_F(NtpExtensionBubbleViewBrowserTest,
                       TestControlledNewTabPageMessageBubble) {
  TestControlledNewTabPageBubbleShown(false);
}

// BrowserUiTest for the warning bubble that appears when opening a new tab and
// an extension is controlling it.
IN_PROC_BROWSER_TEST_F(NtpExtensionBubbleViewBrowserTest,
                       InvokeUi_ntp_override) {
  ShowAndVerifyUi();
}
