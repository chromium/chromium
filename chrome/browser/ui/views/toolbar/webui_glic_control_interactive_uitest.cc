// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_glic_control.h"

#include <memory>
#include <optional>
#include <string>

#include "base/callback_list.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_list_bubble.h"
#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_list_bubble_controller.h"
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
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/glic/glic_actor_task_icon.h"
#include "chrome/browser/ui/views/glic/glic_button.h"
#include "chrome/browser/ui/views/toolbar/test_support/glic_button_test_accessor.h"
#include "chrome/browser/ui/views/toolbar/toolbar_glic_button_interface.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/toolbar/webui_glic_actor_task_icon_control.h"
#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view_test_base.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_page_focus_override.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/menu/menu_controller.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
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
    // Opening and closing the task list bubble changes the browser window's
    // activation. Blink can hit DCHECK(IsFlatTreeClean()) in
    // LayoutSelection::InvalidateStyleAndPaintForSelection() if that reaches
    // the toolbar's page while slot assignment is dirty
    // (https://crbug.com/586727469). Emulated focus keeps activation changes
    // from reaching the page.
    focus_override_ = std::make_unique<content::ScopedPageFocusOverride>(
        GetWebUIWebContents());
  }

  void TearDownOnMainThread() override {
    focus_override_.reset();
    bypass_checks_.reset();
    WebUIToolbarWebViewTestBase::TearDownOnMainThread();
  }

 protected:
  WebUIGlicActorTaskIconControl* GetActorControl() {
    return GetWebUIToolbar()->GetGlicActorTaskIconControl();
  }

  const toolbar_ui_api::mojom::GlicButtonState& GlicState() {
    const WebUIToolbarControlDelegate* delegate = GetWebUIToolbar();
    return *delegate->GetState().glic_button_state;
  }

  const toolbar_ui_api::mojom::GlicActorTaskIconState& ActorIconState() {
    const WebUIToolbarControlDelegate* delegate = GetWebUIToolbar();
    return *delegate->GetState().glic_actor_task_icon_state;
  }

  // Shows the Glic button through ToolbarView and waits for the WebUI to
  // render it.
  [[nodiscard]] bool ShowGlicButton() {
    GetToolbarView()->SetGlicShowState(true);
    return GlicButtonTestAccessor(browser()).WaitForVisible();
  }

  // Waits until the task list bubble is open and the actor task icon is
  // pressed, or until the bubble is closed and the icon is not pressed.
  [[nodiscard]] bool WaitForTaskListBubble(bool showing) {
    return base::test::RunUntil([&]() {
      return GetToolbarView()->IsActivityListBubbleShowing() == showing &&
             GetActorControl()->GetIsPressed() == showing;
    });
  }

  // Creates a task and pauses it from the actor. ToolbarView shows the actor
  // task icon with its attention nudge and opens the task list bubble.
  actor::TaskId CreatePausedTask() {
    auto* actor_service =
        actor::ActorKeyedService::Get(browser()->GetProfile());
    const actor::TaskId task_id = actor_service->CreateTask(
        actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
    actor_service->GetTask(task_id)->Pause(/*from_actor=*/true);
    return task_id;
  }

  void StopTask(actor::TaskId task_id) {
    actor::ActorKeyedService::Get(browser()->GetProfile())
        ->StopTask(task_id, actor::ActorTask::StoppedReason::kStoppedByUser);
  }

  // Moves the mouse over the toolbar. ToolbarView defers nudges while the
  // toolbar is hovered.
  [[nodiscard]] bool HoverToolbar() {
    ToolbarView* toolbar = GetToolbarView();
    return ui_test_utils::SendMouseMoveSync(
               toolbar->GetBoundsInScreen().CenterPoint()) &&
           toolbar->IsMouseHovered();
  }

  // Moves the mouse over the page, off the toolbar.
  [[nodiscard]] bool MoveMouseOffToolbar() {
    return ui_test_utils::SendMouseMoveSync(
        BrowserView::GetBrowserViewForBrowser(browser())
            ->GetContentsView()
            ->GetBoundsInScreen()
            .CenterPoint());
  }

 private:
  std::optional<glic::GlicEnabling::ScopedBypassEnablementChecksForTesting>
      bypass_checks_;
  std::unique_ptr<content::ScopedPageFocusOverride> focus_override_;
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
  glic_button.GetControl()->SetNudgeLabel("Ask Gemini about this page");
  glic_button.GetControl()->SetIsShowingNudge(true);

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

IN_PROC_BROWSER_TEST_F(WebUIGlicControlInteractiveTest,
                       ActorTaskIconControlUpdatesSplitStateAndPreferredSize) {
  ToolbarView* toolbar = GetToolbarView();
  ASSERT_TRUE(ShowGlicButton());
  WebUIToolbarWebView* webui_toolbar = GetWebUIToolbar();
  ASSERT_TRUE(webui_toolbar);
  WebUIGlicActorTaskIconControl* actor_control = GetActorControl();
  ASSERT_TRUE(actor_control);

  EXPECT_FALSE(actor_control->IsVisible());
  EXPECT_FALSE(GlicState().is_split);
  EXPECT_FALSE(ActorIconState().should_show);

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

  // ToolbarView shows and hides the actor task icon and splits the Glic button.
  // The nudge's showing state is set on the control because ToolbarView defers
  // it while the mouse hovers the toolbar.
  //
  // Showing the actor task icon splits the Glic button, updates both states,
  // and adds one button slot, the split Glic width, and the actor task icon's
  // kActorNudgeLabelMargin to the toolbar's preferred width.
  toolbar->ShowGlicSecondaryButton();
  toolbar->SetGlicSecondaryNudgeLabel(u"Needs review");
  actor_control->SetIsShowingNudge(true);
  toolbar->SetGlicSecondaryButtonPressedState(true);

  EXPECT_TRUE(actor_control->IsVisible());
  EXPECT_TRUE(actor_control->GetIsPressed());
  EXPECT_TRUE(GlicState().is_split);
  EXPECT_TRUE(ActorIconState().should_show);
  EXPECT_EQ("Needs review", ActorIconState().nudge_label);
  EXPECT_EQ("Needs review", ActorIconState().tooltip);
  EXPECT_TRUE(ActorIconState().is_pressed);
  EXPECT_EQ(base_preferred_width + slot_delta + split_extra_width,
            webui_toolbar->CalculatePreferredSize({}).width());
  // The WebUI element reports itself as the actor task icon's anchor.
  ASSERT_TRUE(WaitForTrackedElementVisible(kGlicActorTaskIconElementId));

  // Hiding the nudge (`ExecuteHideToolbarNudge` -> `SetIsShowingNudge(false)`)
  // clears the label and restores the default tooltip while keeping the split
  // icon visible.
  actor_control->SetIsShowingNudge(false);
  EXPECT_EQ(std::nullopt, ActorIconState().nudge_label);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_ACTOR_TASK_INDICATOR_TOOLTIP),
            ActorIconState().tooltip);
  EXPECT_TRUE(ActorIconState().should_show);
  EXPECT_EQ(base_preferred_width + slot_delta + split_extra_width,
            webui_toolbar->CalculatePreferredSize({}).width());

  // Hiding the actor task icon unsplits the Glic button and restores the base
  // width.
  toolbar->SetGlicSecondaryButtonPressedState(false);
  toolbar->HideGlicSecondaryButton();

  EXPECT_FALSE(actor_control->IsVisible());
  EXPECT_FALSE(actor_control->GetIsPressed());
  EXPECT_FALSE(GlicState().is_split);
  EXPECT_FALSE(ActorIconState().should_show);
  EXPECT_FALSE(ActorIconState().is_pressed);
  EXPECT_EQ(base_preferred_width,
            webui_toolbar->CalculatePreferredSize({}).width());
  ASSERT_TRUE(WaitForTrackedElementHidden(kGlicActorTaskIconElementId));

  // Unpin Glic: both buttons are hidden and 0 Glic slots are reserved.
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      glic::prefs::kGlicPinnedToTabstrip, false);
  toolbar->SetGlicShowState(false);
  const int unpinned_preferred_width =
      webui_toolbar->CalculatePreferredSize({}).width();
  EXPECT_EQ(base_preferred_width - slot_delta, unpinned_preferred_width);

  // Starting a task while unpinned: ToolbarView shows the Glic button with the
  // actor task icon and the split forces it visible, so both buttons show and
  // reserve 2 slots plus the split Glic and actor task icon widths.
  toolbar->ShowGlicSecondaryButton();
  EXPECT_TRUE(GlicState().should_show);
  EXPECT_TRUE(GlicState().is_split);
  EXPECT_TRUE(ActorIconState().should_show);
  EXPECT_EQ(unpinned_preferred_width + 2 * slot_delta + split_extra_width,
            webui_toolbar->CalculatePreferredSize({}).width());

  // Hiding the Glic button mid-task hides the actor task icon too.
  toolbar->SetGlicShowState(false);
  EXPECT_TRUE(actor_control->IsVisible());
  EXPECT_FALSE(GlicState().should_show);
  EXPECT_FALSE(ActorIconState().should_show);
  EXPECT_EQ(unpinned_preferred_width,
            webui_toolbar->CalculatePreferredSize({}).width());

  // Showing the Glic button again restores the actor task icon. Finishing the
  // task exits split mode, which hides the unpinned Glic button.
  toolbar->SetGlicShowState(true);
  EXPECT_TRUE(GlicState().should_show);
  EXPECT_TRUE(ActorIconState().should_show);
  toolbar->HideGlicSecondaryButton();
  EXPECT_FALSE(GlicState().is_split);
  EXPECT_FALSE(GlicState().should_show);
  EXPECT_FALSE(ActorIconState().should_show);
  EXPECT_EQ(unpinned_preferred_width,
            webui_toolbar->CalculatePreferredSize({}).width());

  // Changing the tab strip orientation after the task finished does not show
  // the actor task icon again.
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  EXPECT_FALSE(actor_control->IsVisible());
  EXPECT_FALSE(ActorIconState().should_show);
}

IN_PROC_BROWSER_TEST_F(WebUIGlicControlInteractiveTest,
                       ActorTaskLifecycleDrivesSplitButtonAndTaskListBubble) {
#if BUILDFLAG(IS_OZONE)
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP()
        << "Wayland does not update global cursor screen position in tests";
  }
#endif

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));
  ASSERT_TRUE(MoveMouseOffToolbar());

  ToolbarView* toolbar = GetToolbarView();
  ASSERT_EQ(
      toolbar,
      glic::GlicSplitButtonController::From(browser())->GetActiveDelegate());
  ASSERT_TRUE(ShowGlicButton());
  WebUIGlicActorTaskIconControl* actor_control = GetActorControl();
  ASSERT_TRUE(actor_control);

  auto* actor_service = actor::ActorKeyedService::Get(browser()->GetProfile());
  auto* activity_manager =
      glic::LegacyGlicActivityManager::Get(browser()->GetProfile());
  ASSERT_TRUE(actor_service);
  ASSERT_TRUE(activity_manager);

  // 1. Start a task: ToolbarView enters split mode and shows the actor task
  // icon without opening the task list bubble yet.
  const actor::TaskId task_id = actor_service->CreateTask(
      actor::TestTaskSourceInfo(), actor::NoEnterprisePolicyChecker());
  activity_manager->UpdateTaskIconComponents(task_id);

  EXPECT_TRUE(GlicState().is_split);
  EXPECT_TRUE(ActorIconState().should_show);
  EXPECT_FALSE(toolbar->IsActivityListBubbleShowing());
  EXPECT_FALSE(actor_control->GetIsPressed());

  // 2. Pause the task from the actor: ToolbarView shows the attention nudge
  // label and auto-opens the real ActorTaskListBubble with the actor icon
  // pressed. Only the actor task icon is highlighted as the anchor.
  actor_service->GetTask(task_id)->Pause(/*from_actor=*/true);

  const std::string expected_check_task_label =
      base::UTF16ToUTF8(l10n_util::GetPluralStringFUTF16(
          IDS_ACTOR_TASK_NUDGE_CHECK_TASK_LABEL, 1));
  ASSERT_TRUE(WaitForTaskListBubble(/*showing=*/true));
  EXPECT_TRUE(toolbar->GetIsShowingGlicSecondaryNudge());
  EXPECT_EQ(expected_check_task_label, ActorIconState().nudge_label);
  EXPECT_TRUE(ActorIconState().is_pressed);
  auto has_anchor_highlight = [&](const std::string& selector) {
    return content::EvalJs(GetWebUIWebContents(),
                           base::StrCat({"!!", GetButtonAppJS(selector),
                                         "?.classList.contains("
                                         "'anchor-highlight')"}))
        .ExtractBool();
  };
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return has_anchor_highlight("#glic-actor-task-icon"); }));
  EXPECT_FALSE(has_anchor_highlight("#glic-button"));

  // 3. Closing the bubble resets the pressed state once the widget is
  // destroyed.
  toolbar->CloseActivityListBubble();
  ASSERT_TRUE(WaitForTaskListBubble(/*showing=*/false));
  EXPECT_FALSE(ActorIconState().is_pressed);

  // 4. Clicking the unpressed actor task icon re-opens the real
  // ActorTaskListBubble and logs the NeedsAttention click user action. The
  // bubble is only shown in an active browser window, and the window is
  // reactivated asynchronously after the bubble closes.
  ASSERT_TRUE(base::test::RunUntil([&]() { return browser()->IsActive(); }));
  auto* bubble_controller =
      glic::GlicActivityListBubbleController::From(browser());
  ASSERT_TRUE(bubble_controller);
  int bubble_shown_count = 0;
  base::CallbackListSubscription bubble_shown_subscription =
      bubble_controller->RegisterBubbleShownCallback(
          base::BindLambdaForTesting([&]() { ++bubble_shown_count; }));
  base::UserActionTester user_action_tester;
  ASSERT_TRUE(content::ExecJs(
      GetWebUIWebContents(),
      base::StrCat({GetButtonAppJS("#glic-actor-task-icon"),
                    ".shadowRoot.querySelector('#button').click();"})));
  ASSERT_TRUE(WaitForTaskListBubble(/*showing=*/true));
  EXPECT_EQ(1, bubble_shown_count);
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Actor.Ui.GlobalTaskIndicator.NeedsAttention.Click"));

  // 5. Activating while already pressed (e.g. via keyboard) does not re-open
  // the bubble. Close the bubble via ToolbarView to verify teardown.
  GetWebUIToolbar()->OnGlicActorTaskIconClicked();
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "Actor.Ui.GlobalTaskIndicator.NeedsAttention.Click"));
  EXPECT_EQ(1, bubble_shown_count);
  EXPECT_TRUE(toolbar->IsActivityListBubbleShowing());
  EXPECT_TRUE(actor_control->GetIsPressed());

  toolbar->CloseActivityListBubble();
  ASSERT_TRUE(WaitForTaskListBubble(/*showing=*/false));

  // 6. Resuming the task hides the attention nudge label while keeping the
  // split button visible.
  actor_service->GetTask(task_id)->Resume();
  activity_manager->UpdateTaskIconComponents(task_id);
  EXPECT_FALSE(toolbar->GetIsShowingGlicSecondaryNudge());
  EXPECT_EQ(std::nullopt, ActorIconState().nudge_label);
  EXPECT_TRUE(GlicState().is_split);
  EXPECT_TRUE(ActorIconState().should_show);

  // 7. Stopping (cancelling) the task hides the actor task icon and exits split
  // mode.
  StopTask(task_id);
  activity_manager->UpdateTaskIconComponents(task_id);
  EXPECT_FALSE(actor_control->IsVisible());
  EXPECT_FALSE(GlicState().is_split);
  EXPECT_FALSE(ActorIconState().should_show);
}

// The task list bubble can be requested in the same step that shows the actor
// task icon, before the WebUI has reported the icon's element. The bubble opens
// once the element is available, and the icon is pressed while it is open.
IN_PROC_BROWSER_TEST_F(WebUIGlicControlInteractiveTest,
                       TaskListBubbleOpensWhenActorTaskIconElementAppears) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ToolbarView* toolbar = GetToolbarView();
  ASSERT_EQ(
      toolbar,
      glic::GlicSplitButtonController::From(browser())->GetActiveDelegate());
  ASSERT_TRUE(ShowGlicButton());
  WebUIGlicActorTaskIconControl* actor_control = GetActorControl();
  auto* bubble_controller =
      glic::GlicActivityListBubbleController::From(browser());
  ASSERT_TRUE(bubble_controller);
  auto bubble_element = [&]() {
    return ui::ElementTracker::GetElementTracker()->GetElementInAnyContext(
        kActorTaskListBubbleView);
  };

  // A paused task provides a row for the bubble. Pausing shows the icon and
  // requests the bubble in the same step, so the bubble waits for its anchor.
  const actor::TaskId task_id = CreatePausedTask();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetTrackedElement(kGlicActorTaskIconElementId) && bubble_element();
  }));
  toolbar->CloseActivityListBubble();
  ASSERT_TRUE(WaitForTaskListBubble(/*showing=*/false));

  // The window is reactivated asynchronously after the bubble closes. The
  // bubble is only shown in an active browser window, and the icon's element
  // is removed only after the WebUI has seen the activation change.
  ASSERT_TRUE(base::test::RunUntil([&]() { return browser()->IsActive(); }));
  toolbar->HideGlicSecondaryButton();
  ASSERT_TRUE(WaitForTrackedElementHidden(kGlicActorTaskIconElementId));

  // Show the icon and request the bubble before the element is reported. The
  // bubble opens once the element exists and the icon is pressed.
  int bubble_shown_count = 0;
  base::CallbackListSubscription bubble_shown_subscription =
      bubble_controller->RegisterBubbleShownCallback(
          base::BindLambdaForTesting([&]() { ++bubble_shown_count; }));
  toolbar->ShowGlicSecondaryButton();
  bubble_controller->ShowBubble();
  EXPECT_FALSE(GetTrackedElement(kGlicActorTaskIconElementId));
  EXPECT_FALSE(bubble_element());
  EXPECT_FALSE(toolbar->IsActivityListBubbleShowing());
  EXPECT_FALSE(actor_control->GetIsPressed());
  EXPECT_EQ(0, bubble_shown_count);

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetTrackedElement(kGlicActorTaskIconElementId) && bubble_element();
  }));
  EXPECT_TRUE(toolbar->IsActivityListBubbleShowing());
  EXPECT_TRUE(actor_control->GetIsPressed());
  EXPECT_EQ(1, bubble_shown_count);

  toolbar->CloseActivityListBubble();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !toolbar->IsActivityListBubbleShowing() &&
           !actor_control->GetIsPressed() && !bubble_element();
  }));

  // The bubble does not open once the element exists if it was closed while
  // it waited for its anchor. The request is made once the browser window is
  // active again.
  ASSERT_TRUE(base::test::RunUntil([&]() { return browser()->IsActive(); }));
  toolbar->HideGlicSecondaryButton();
  ASSERT_TRUE(WaitForTrackedElementHidden(kGlicActorTaskIconElementId));
  toolbar->ShowGlicSecondaryButton();
  bubble_controller->ShowBubble();
  toolbar->CloseActivityListBubble();
  ASSERT_TRUE(WaitForTrackedElementVisible(kGlicActorTaskIconElementId));
  EXPECT_FALSE(toolbar->IsActivityListBubbleShowing());
  EXPECT_FALSE(bubble_element());
  EXPECT_FALSE(actor_control->GetIsPressed());
  EXPECT_EQ(1, bubble_shown_count);

  StopTask(task_id);
}

// A bubble that is open is replaced when the task list changes. The icon does
// not stay pressed if the replacement is dropped because the icon cannot show.
IN_PROC_BROWSER_TEST_F(
    WebUIGlicControlInteractiveTest,
    ActorTaskIconNotPressedWhenTaskListBubbleRefreshDropped) {
  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ToolbarView* toolbar = GetToolbarView();
  ASSERT_EQ(
      toolbar,
      glic::GlicSplitButtonController::From(browser())->GetActiveDelegate());
  ASSERT_TRUE(ShowGlicButton());
  auto* bubble_controller =
      glic::GlicActivityListBubbleController::From(browser());
  ASSERT_TRUE(bubble_controller);

  const actor::TaskId task_id = CreatePausedTask();
  ASSERT_TRUE(WaitForTaskListBubble(/*showing=*/true));

  // Hiding the Glic button hides the actor task icon and leaves the bubble
  // open.
  toolbar->SetGlicShowState(false);
  ASSERT_FALSE(GetActorControl()->ShouldShow());
  ASSERT_TRUE(toolbar->IsActivityListBubbleShowing());

  bubble_controller->OnStateUpdate(/*is_start_notification=*/false);
  ASSERT_TRUE(WaitForTaskListBubble(/*showing=*/false));

  StopTask(task_id);
}

IN_PROC_BROWSER_TEST_F(WebUIGlicControlInteractiveTest,
                       ToolbarViewHoverLockDefersWebUINudges) {
#if BUILDFLAG(IS_OZONE)
  if (::ui::OzonePlatform::RunningOnWaylandForTest()) {
    GTEST_SKIP()
        << "Wayland does not update global cursor screen position in tests";
  }
#endif

  ASSERT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

  ToolbarView* toolbar = GetToolbarView();
  ASSERT_TRUE(ShowGlicButton());

  // 1. Hover the toolbar and trigger a Glic nudge: showing is deferred until
  // the mouse leaves the toolbar.
  ASSERT_TRUE(HoverToolbar());

  toolbar->OnTriggerGlicNudgeUI(glic::NudgeParams("Hover-locked Glic nudge"));
  EXPECT_FALSE(toolbar->GetIsShowingGlicNudge());
  EXPECT_EQ(std::nullopt, GlicState().nudge_label);

  ASSERT_TRUE(MoveMouseOffToolbar());
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return toolbar->GetIsShowingGlicNudge(); }));
  EXPECT_EQ("Hover-locked Glic nudge", GlicState().nudge_label);

  // 2. Hover the toolbar and hide the Glic nudge: hiding is deferred until the
  // mouse leaves the toolbar.
  ASSERT_TRUE(HoverToolbar());

  toolbar->OnHideGlicNudgeUI();
  EXPECT_TRUE(toolbar->GetIsShowingGlicNudge());
  EXPECT_EQ("Hover-locked Glic nudge", GlicState().nudge_label);

  ASSERT_TRUE(MoveMouseOffToolbar());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !toolbar->GetIsShowingGlicNudge(); }));
  EXPECT_EQ(std::nullopt, GlicState().nudge_label);

  // 3. Show a Glic nudge, then hover the toolbar and trigger an actor nudge:
  // entering split mode hides the Glic nudge, while the actor nudge label
  // stays deferred until the mouse leaves the toolbar.
  toolbar->OnTriggerGlicNudgeUI(glic::NudgeParams("Glic nudge"));
  ASSERT_TRUE(toolbar->GetIsShowingGlicNudge());
  ASSERT_TRUE(HoverToolbar());

  toolbar->TriggerGlicSecondaryNudge(u"Needs review");
  EXPECT_TRUE(GlicState().is_split);
  EXPECT_FALSE(toolbar->GetIsShowingGlicNudge());
  EXPECT_EQ(std::nullopt, GlicState().nudge_label);
  EXPECT_FALSE(toolbar->GetIsShowingGlicSecondaryNudge());
  EXPECT_EQ(std::nullopt, ActorIconState().nudge_label);

  ASSERT_TRUE(MoveMouseOffToolbar());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return toolbar->GetIsShowingGlicSecondaryNudge(); }));
  EXPECT_EQ("Needs review", ActorIconState().nudge_label);

  // 4. Hover the toolbar while the actor nudge is showing and return to the
  // default task icon state: hiding the actor nudge is deferred until mouse
  // exit while keeping split mode active.
  ASSERT_TRUE(HoverToolbar());

  toolbar->ShowGlicSecondaryButton();
  EXPECT_TRUE(toolbar->GetIsShowingGlicSecondaryNudge());
  EXPECT_EQ("Needs review", ActorIconState().nudge_label);

  ASSERT_TRUE(MoveMouseOffToolbar());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !toolbar->GetIsShowingGlicSecondaryNudge(); }));
  EXPECT_EQ(std::nullopt, ActorIconState().nudge_label);
  EXPECT_TRUE(GlicState().is_split);

  toolbar->HideGlicSecondaryButton();
  EXPECT_FALSE(GlicState().is_split);
  EXPECT_FALSE(toolbar->GetIsShowingGlicSecondaryNudge());

  // 5. Hover the toolbar, trigger a Glic nudge and show the actor task icon:
  // the deferred Glic nudge shows on the split Glic half once the mouse leaves
  // the toolbar.
  ASSERT_TRUE(HoverToolbar());

  toolbar->OnTriggerGlicNudgeUI(glic::NudgeParams("Deferred Glic nudge"));
  toolbar->ShowGlicSecondaryButton();
  EXPECT_TRUE(GlicState().is_split);
  EXPECT_FALSE(toolbar->GetIsShowingGlicNudge());
  EXPECT_EQ(std::nullopt, GlicState().nudge_label);

  ASSERT_TRUE(MoveMouseOffToolbar());
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return toolbar->GetIsShowingGlicNudge(); }));
  EXPECT_EQ("Deferred Glic nudge", GlicState().nudge_label);
  EXPECT_TRUE(GlicState().is_split);

  // 6. Showing the actor task icon again while split hides the Glic nudge.
  toolbar->ShowGlicSecondaryButton();
  EXPECT_FALSE(toolbar->GetIsShowingGlicNudge());
  EXPECT_EQ(std::nullopt, GlicState().nudge_label);
  EXPECT_TRUE(GlicState().is_split);

  toolbar->HideGlicSecondaryButton();
  EXPECT_FALSE(GlicState().is_split);
}

// With horizontal tabs and no toolbar Glic button, the tab strip shows the Glic
// button and ToolbarView keeps its own Glic button and actor task icon hidden.
class WebUIGlicControlNoHorizontalTabToolbarButtonInteractiveTest
    : public WebUIGlicControlInteractiveTest {
 public:
  WebUIGlicControlNoHorizontalTabToolbarButtonInteractiveTest() {
    feature_list_.InitAndDisableFeature(
        features::kGlicHorizontalTabToolbarButton);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    WebUIGlicControlNoHorizontalTabToolbarButtonInteractiveTest,
    ActorTaskIconStaysHiddenAfterTaskEndsWhileHidden) {
  ToolbarView* toolbar = GetToolbarView();
  toolbar->SetGlicShowState(true);

  // A task starts and ends while the toolbar's actor task icon is hidden.
  toolbar->ShowGlicSecondaryButton();
  EXPECT_FALSE(GetActorControl()->IsVisible());
  EXPECT_TRUE(GlicState().is_split);
  toolbar->HideGlicSecondaryButton();
  EXPECT_FALSE(GlicState().is_split);

  // Switching to vertical tabs shows the toolbar's Glic button, but not the
  // actor task icon of the task that ended.
  tabs::VerticalTabStripStateController::From(browser())
      ->SetVerticalTabsEnabled(true);
  EXPECT_TRUE(GlicState().should_show);
  EXPECT_FALSE(GetActorControl()->IsVisible());
  EXPECT_FALSE(ActorIconState().should_show);
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
