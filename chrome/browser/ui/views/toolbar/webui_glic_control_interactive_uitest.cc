// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_glic_control.h"

#include <optional>
#include <string>

#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/toolbar/test_support/glic_button_test_accessor.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_button_interface.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view_test_base.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/menu/menu_controller.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

class WebUIGlicControlInteractiveTest : public WebUIToolbarWebViewTestBase {
 public:
  WebUIGlicControlInteractiveTest()
      : WebUIToolbarWebViewTestBase(
            {features::kInitialWebUI, features::kWebUIToolbar,
             ::features::kWebUIGlicButton, features::kGlic,
             features::kGlicHorizontalTabToolbarButton,
             features::kToolbarGlicButtonResizing,
#if BUILDFLAG(IS_CHROMEOS)
             chromeos::features::kFeatureManagementGlic,
#endif
             features::kSkipIPCChannelPausingForNonGuests,
             features::kWebUIInProcessResourceLoadingV2},
            {}) {
  }

  void SetUpOnMainThread() override {
    WebUIToolbarWebViewTestBase::SetUpOnMainThread();
    bypass_checks_.emplace();
  }

  void TearDownOnMainThread() override {
    bypass_checks_.reset();
    WebUIToolbarWebViewTestBase::TearDownOnMainThread();
  }

 private:
  std::optional<glic::GlicEnabling::ScopedBypassEnablementChecksForTesting>
      bypass_checks_;
};

IN_PROC_BROWSER_TEST_F(WebUIGlicControlInteractiveTest,
                       GlicButtonClickedAndRightClicked) {
  GlicButtonTestAccessor glic_button(browser());
  ASSERT_TRUE(glic_button.GetControl());
  glic_button.GetControl()->SetVisible(true);

  // 1. Verify Glic button is visible in WebUI and tracked by ElementTracker.
  ASSERT_TRUE(glic_button.WaitForVisible());

  int element_activated_count = 0;
  auto activated_subscription =
      ui::ElementTracker::GetElementTracker()
          ->AddElementActivatedInAnyContextCallback(
              kGlicButtonElementId,
              base::BindLambdaForTesting(
                  [&](ui::TrackedElement*) { ++element_activated_count; }));

  EXPECT_FALSE(glic_button.IsMenuOpen());

  const std::string initial_aria_label = glic_button.GetAriaLabel();
  EXPECT_FALSE(initial_aria_label.empty());

  // 2. Click the Glic button and verify it opens the Glic panel, notifies
  // TrackedElementManager.notifyElementActivated, and updates the WebUI button
  // state and aria-label to the close tooltip.
  glic_button.Click();

  // 3. Verify panel open state, ElementTracker activation, and aria-label
  // reflection in WebUI.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return element_activated_count == 1 && glic_button.IsMenuOpen() &&
           glic_button.IsAriaExpanded() &&
           !glic_button.GetAriaLabel().empty() &&
           glic_button.GetAriaLabel() != initial_aria_label &&
           glic_button.GetAriaLabel() == glic_button.GetTooltip();
  }));

  // 4. Close panel and verify state reset.
  glic_button.GetControl()->SetGlicPanelIsOpen(false);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !glic_button.IsMenuOpen() && !glic_button.IsAriaExpanded() &&
           glic_button.GetAriaLabel() == initial_aria_label;
  }));

  // 5. Right-click (contextmenu) the Glic button to show the context menu.
  glic_button.RightClick();

  // 6. Verify context menu is running and button is highlighted in WebUI.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return views::MenuController::GetActiveInstance() != nullptr &&
           glic_button.IsMenuOpen();
  }));

  // 7. Close context menu and verify it is dismissed and highlight is removed.
  auto* menu_controller = views::MenuController::GetActiveInstance();
  ASSERT_TRUE(menu_controller);
  menu_controller->Cancel(views::MenuController::ExitType::kAll);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return views::MenuController::GetActiveInstance() == nullptr &&
           !glic_button.IsMenuOpen();
  }));

  // 8. Hide the button and verify WebUI DOM update.
  glic_button.GetControl()->SetVisible(false);
  ASSERT_TRUE(glic_button.WaitForHidden());
}

IN_PROC_BROWSER_TEST_F(WebUIGlicControlInteractiveTest,
                       ResponsiveCollapseAndStateTransitions) {
  GlicButtonTestAccessor glic_button(browser());
  ASSERT_TRUE(glic_button.GetControl());
  glic_button.GetControl()->SetVisible(true);

  // 1. Wait for Glic button to be visible and expanded with its label.
  ASSERT_TRUE(glic_button.WaitForVisible());
  ASSERT_TRUE(glic_button.WaitForCollapsed(false));

  // 2. Constrain toolbar space so Glic's label collapses to icon-only while the
  // button itself stays visible.
  ASSERT_EQ(true, SetSpacerWidth(GetWebUIToolbar()->bounds().width()));
  ASSERT_TRUE(glic_button.WaitForCollapsed(true));
  EXPECT_TRUE(glic_button.IsVisible());

  // 3. Trigger a nudge while collapsed: verify the button remains collapsed
  // with the label hidden, while aria-label updates to the nudge text.
  glic_button.GetControl()->SetIsShowingNudge(true);
  glic_button.GetControl()->SetNudgeLabel("Ask Gemini about this page");

  ASSERT_TRUE(glic_button.WaitForAriaLabel("Ask Gemini about this page"));
  EXPECT_TRUE(glic_button.IsCollapsed());

  // 4. Open the Glic panel while collapsed: verify the button remains
  // collapsed, aria-expanded and is-menu-open update, and aria-label switches
  // to the close tooltip string.
  glic_button.GetControl()->SetIsShowingNudge(false);
  glic_button.GetControl()->SetGlicPanelIsOpen(true);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return glic_button.IsCollapsed() && glic_button.IsMenuOpen() &&
           glic_button.IsAriaExpanded() &&
           !glic_button.GetAriaLabel().empty() &&
           glic_button.GetAriaLabel() == glic_button.GetTooltip();
  }));

  // 5. Restore available toolbar width and verify the label expands again.
  ASSERT_EQ(true, SetSpacerWidth(0));
  ASSERT_TRUE(glic_button.WaitForCollapsed(false));
}
