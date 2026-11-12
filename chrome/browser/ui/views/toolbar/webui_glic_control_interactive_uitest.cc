// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_glic_control.h"

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_controller.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/service/legacy_glic_activity_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/glic/glic_actor_task_icon.h"
#include "chrome/browser/ui/views/glic/glic_button.h"
#include "chrome/browser/ui/views/toolbar/test_support/glic_button_test_accessor.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_button_interface.h"
#include "chrome/browser/ui/views/toolbar/webui_glic_actor_task_icon_control.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view_test_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
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

namespace {

class FakeGlicSplitButtonDelegate : public glic::GlicSplitButtonDelegate {
 public:
  void ShowActivityListBubble() override { is_bubble_showing_ = true; }
  void CloseActivityListBubble() override { is_bubble_showing_ = false; }
  bool IsActivityListBubbleShowing() override { return is_bubble_showing_; }

 private:
  bool is_bubble_showing_ = false;
};

class ScopedVerticalTabsDelegateOverride {
 public:
  ScopedVerticalTabsDelegateOverride(
      glic::GlicSplitButtonController* controller,
      glic::GlicSplitButtonDelegate* delegate)
      : controller_(controller),
        previous_delegate_(controller->GetActiveDelegate()) {
    controller_->SetVerticalTabsDelegate(delegate);
  }
  ~ScopedVerticalTabsDelegateOverride() {
    controller_->SetVerticalTabsDelegate(previous_delegate_);
  }

 private:
  const raw_ptr<glic::GlicSplitButtonController> controller_;
  const raw_ptr<glic::GlicSplitButtonDelegate> previous_delegate_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(WebUIGlicControlInteractiveTest,
                       ActorTaskIconControlUpdatesSplitStateAndPreferredSize) {
  GlicButtonTestAccessor glic_button(browser());
  ASSERT_TRUE(glic_button.GetControl());
  glic_button.GetControl()->SetVisible(true);
  ASSERT_TRUE(glic_button.WaitForVisible());

  WebUIToolbarWebView* webui_toolbar = GetWebUIToolbar();
  ASSERT_TRUE(webui_toolbar);
  WebUIToolbarControlDelegate* toolbar_delegate = webui_toolbar;
  WebUIGlicActorTaskIconControl* actor_control =
      webui_toolbar->GetGlicActorTaskIconControl();
  ASSERT_TRUE(actor_control);

  EXPECT_FALSE(actor_control->IsVisible());
  EXPECT_FALSE(toolbar_delegate->GetState().glic_button_state->is_split);
  EXPECT_FALSE(
      toolbar_delegate->GetState().glic_actor_task_icon_state->should_show);

  const int slot_delta =
      GetLayoutConstant(LayoutConstant::kToolbarButtonHeight) +
      GetLayoutConstant(LayoutConstant::kToolbarIconDefaultMargin);
  // Beyond their slots, the split Glic half spans Views' collapsed width, and
  // the actor task icon is kActorNudgeLabelMargin wider than a square button,
  // with or without a nudge.
  const int split_extra_width =
      glic::kCollapsedWidth - slot_delta + glic::kActorNudgeLabelMargin;
  const int base_preferred_width =
      webui_toolbar->CalculatePreferredSize({}).width();

  // The Glic and actor task icon controls are independent. ToolbarView splits
  // the Glic button while the actor task icon is shown, and calls
  // SetCanShow(false) on the icon while the Glic button is hidden. This test
  // makes those calls directly.
  auto* glic_control =
      static_cast<WebUIGlicControl*>(webui_toolbar->GetGlicControl());

  // Showing the actor task icon and splitting the Glic button updates both
  // states and adds one button slot, the split Glic width, and the actor task
  // icon's kActorNudgeLabelMargin to the toolbar's preferred width.
  actor_control->SetVisible(true);
  glic_control->SetIsSplit(true);
  actor_control->ShowNudgeLabel(u"Needs review");
  actor_control->SetIsShowingNudge(true);
  actor_control->SetPressedState(true);

  EXPECT_TRUE(actor_control->IsVisible());
  EXPECT_TRUE(actor_control->GetIsPressed());
  EXPECT_TRUE(toolbar_delegate->GetState().glic_button_state->is_split);
  EXPECT_TRUE(
      toolbar_delegate->GetState().glic_actor_task_icon_state->should_show);
  EXPECT_EQ(
      "Needs review",
      toolbar_delegate->GetState().glic_actor_task_icon_state->nudge_label);
  EXPECT_EQ("Needs review",
            toolbar_delegate->GetState().glic_actor_task_icon_state->tooltip);
  EXPECT_TRUE(
      toolbar_delegate->GetState().glic_actor_task_icon_state->is_pressed);
  EXPECT_EQ(base_preferred_width + slot_delta + split_extra_width,
            webui_toolbar->CalculatePreferredSize({}).width());
  // The WebUI element reports itself as the actor task icon's anchor.
  ASSERT_TRUE(WaitForTrackedElementVisible(kGlicActorTaskIconElementId));

  // Hiding the nudge (`ExecuteHideToolbarNudge` -> `SetIsShowingNudge(false)`)
  // clears the label and restores the default tooltip while keeping the split
  // icon visible.
  actor_control->SetIsShowingNudge(false);
  EXPECT_EQ(
      std::nullopt,
      toolbar_delegate->GetState().glic_actor_task_icon_state->nudge_label);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_ACTOR_TASK_INDICATOR_TOOLTIP),
            toolbar_delegate->GetState().glic_actor_task_icon_state->tooltip);
  EXPECT_TRUE(
      toolbar_delegate->GetState().glic_actor_task_icon_state->should_show);
  EXPECT_EQ(base_preferred_width + slot_delta + split_extra_width,
            webui_toolbar->CalculatePreferredSize({}).width());

  // Hiding the actor task icon and unsplitting the Glic button restores the
  // base width.
  actor_control->SetPressedState(false);
  actor_control->SetTaskIconToDefault();
  actor_control->SetVisible(false);
  glic_control->SetIsSplit(false);

  EXPECT_FALSE(actor_control->IsVisible());
  EXPECT_FALSE(actor_control->GetIsPressed());
  EXPECT_FALSE(toolbar_delegate->GetState().glic_button_state->is_split);
  EXPECT_FALSE(
      toolbar_delegate->GetState().glic_actor_task_icon_state->should_show);
  EXPECT_FALSE(
      toolbar_delegate->GetState().glic_actor_task_icon_state->is_pressed);
  EXPECT_EQ(base_preferred_width,
            webui_toolbar->CalculatePreferredSize({}).width());
  ASSERT_TRUE(WaitForTrackedElementHidden(kGlicActorTaskIconElementId));

  // Unpin Glic: both buttons are hidden and 0 Glic slots are reserved.
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicPinnedToTabstrip, false);
  glic_button.GetControl()->SetVisible(false);
  const int unpinned_preferred_width =
      webui_toolbar->CalculatePreferredSize({}).width();
  EXPECT_EQ(base_preferred_width - slot_delta, unpinned_preferred_width);

  // Starting a task while unpinned: the split forces the Glic button visible,
  // so both buttons show and reserve 2 slots plus the split Glic and actor
  // task icon widths.
  actor_control->SetVisible(true);
  glic_control->SetIsSplit(true);
  glic_button.GetControl()->SetVisible(true);
  EXPECT_TRUE(toolbar_delegate->GetState().glic_button_state->should_show);
  EXPECT_TRUE(toolbar_delegate->GetState().glic_button_state->is_split);
  EXPECT_TRUE(
      toolbar_delegate->GetState().glic_actor_task_icon_state->should_show);
  EXPECT_EQ(unpinned_preferred_width + 2 * slot_delta + split_extra_width,
            webui_toolbar->CalculatePreferredSize({}).width());

  // If the Glic button is hidden mid-task (e.g. GlicButtonController calls
  // SetGlicShowState(false)), SetCanShow(false) hides the actor task icon.
  glic_button.GetControl()->SetVisible(false);
  actor_control->SetCanShow(false);
  EXPECT_TRUE(actor_control->IsVisible());
  EXPECT_FALSE(toolbar_delegate->GetState().glic_button_state->should_show);
  EXPECT_FALSE(
      toolbar_delegate->GetState().glic_actor_task_icon_state->should_show);
  EXPECT_EQ(unpinned_preferred_width,
            webui_toolbar->CalculatePreferredSize({}).width());

  // Showing the Glic button again restores the actor task icon. Finishing the
  // task exits split mode, which hides the unpinned Glic button.
  glic_button.GetControl()->SetVisible(true);
  actor_control->SetCanShow(true);
  EXPECT_TRUE(toolbar_delegate->GetState().glic_button_state->should_show);
  EXPECT_TRUE(
      toolbar_delegate->GetState().glic_actor_task_icon_state->should_show);
  actor_control->SetVisible(false);
  glic_control->SetIsSplit(false);
  EXPECT_FALSE(toolbar_delegate->GetState().glic_button_state->is_split);
  EXPECT_FALSE(toolbar_delegate->GetState().glic_button_state->should_show);
  EXPECT_FALSE(
      toolbar_delegate->GetState().glic_actor_task_icon_state->should_show);
  EXPECT_EQ(unpinned_preferred_width,
            webui_toolbar->CalculatePreferredSize({}).width());
}

IN_PROC_BROWSER_TEST_F(WebUIGlicControlInteractiveTest,
                       ActorTaskIconClickedShowsAndClosesTaskListBubble) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  auto* split_controller = glic::GlicSplitButtonController::From(browser());
  ASSERT_TRUE(split_controller);
  FakeGlicSplitButtonDelegate fake_delegate;
  ScopedVerticalTabsDelegateOverride delegate_override(split_controller,
                                                       &fake_delegate);

  auto* actor_service = actor::ActorKeyedService::Get(browser()->GetProfile());
  auto* activity_manager =
      glic::LegacyGlicActivityManager::Get(browser()->GetProfile());
  ASSERT_TRUE(actor_service);
  ASSERT_TRUE(activity_manager);

  const actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  actor_service->GetTask(task_id)->Pause(/*from_actor=*/true);
  activity_manager->UpdateTaskIconComponents(task_id);
  fake_delegate.CloseActivityListBubble();
  ASSERT_FALSE(fake_delegate.IsActivityListBubbleShowing());

  WebUIToolbarWebView* webui_toolbar = GetWebUIToolbar();
  ASSERT_TRUE(webui_toolbar);
  WebUIGlicActorTaskIconControl* actor_control =
      webui_toolbar->GetGlicActorTaskIconControl();
  ASSERT_TRUE(actor_control);
  actor_control->SetVisible(true);
  ASSERT_FALSE(actor_control->GetIsPressed());
  ASSERT_TRUE(WaitForTrackedElementVisible(kGlicActorTaskIconElementId));

  base::UserActionTester user_action_tester;
  ASSERT_TRUE(
      content::ExecJs(GetWebUIWebContents(),
                      "document.querySelector('toolbar-app').shadowRoot"
                      "    .querySelector('#glic-actor-task-icon').shadowRoot"
                      "    .querySelector('#button').click();"));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_delegate.IsActivityListBubbleShowing(); }));
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Actor.Ui.GlobalTaskIndicator.NeedsAttention.Click"));

  // A click while the icon is pressed does not reopen the bubble, but is still
  // logged.
  fake_delegate.CloseActivityListBubble();
  actor_control->SetPressedState(true);
  webui_toolbar->OnGlicActorTaskIconClicked();
  EXPECT_FALSE(fake_delegate.IsActivityListBubbleShowing());
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "Actor.Ui.GlobalTaskIndicator.NeedsAttention.Click"));

  actor_service->StopTask(task_id,
                          actor::ActorTask::StoppedReason::kTaskComplete);
}

class WebUIGlicControlActorUiDisabledInteractiveTest
    : public WebUIToolbarWebViewTestBase {
 public:
  WebUIGlicControlActorUiDisabledInteractiveTest()
      : WebUIToolbarWebViewTestBase(
            {features::kInitialWebUI, features::kWebUIToolbar,
             ::features::kWebUIGlicButton, features::kGlic,
             features::kGlicHorizontalTabToolbarButton,
#if BUILDFLAG(IS_CHROMEOS)
             chromeos::features::kFeatureManagementGlic,
#endif
             features::kSkipIPCChannelPausingForNonGuests,
             features::kWebUIInProcessResourceLoadingV2},
            {features::kGlicActorUi}) {
  }
};

IN_PROC_BROWSER_TEST_F(WebUIGlicControlActorUiDisabledInteractiveTest,
                       ActorTaskIconNotInitializedWhenActorUiDisabled) {
  WebUIToolbarWebView* webui_toolbar = GetWebUIToolbar();
  ASSERT_TRUE(webui_toolbar);
  WebUIToolbarControlDelegate* toolbar_delegate = webui_toolbar;
  EXPECT_TRUE(
      toolbar_delegate->GetState().glic_actor_task_icon_state->tooltip.empty());
}
