// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_observer.h"
#include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "chrome/browser/ui/performance_controls/performance_intervention_button_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/performance_controls/performance_intervention_bubble.h"
#include "chrome/browser/ui/views/performance_controls/performance_intervention_button.h"
#include "chrome/browser/ui/views/performance_controls/tab_list_row_view.h"
#include "chrome/browser/ui/views/performance_controls/tab_list_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "ui/base/interaction/interaction_test_util.h"
#include "ui/base/ozone_buildflags.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kSecondTab);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kThirdTab);
constexpr char kSkipPixelTestsReason[] = "Should only run in pixel_tests.";
constexpr char kMessageTriggerResultHistogram[] =
    "PerformanceControls.Intervention.BackgroundTab.Cpu.MessageTriggerResult";

class DiscardWaiter : public resource_coordinator::TabLifecycleObserver {
 public:
  DiscardWaiter() {
    run_loop_ = std::make_unique<base::RunLoop>(
        base::RunLoop::Type::kNestableTasksAllowed);
    resource_coordinator::TabLifecycleUnitExternal::AddTabLifecycleObserver(
        this);
  }

  ~DiscardWaiter() override {
    resource_coordinator::TabLifecycleUnitExternal::RemoveTabLifecycleObserver(
        this);
  }

  void Wait() { run_loop_->Run(); }

  void OnDiscardedStateChange(content::WebContents* contents,
                              LifecycleUnitDiscardReason reason,
                              bool is_discarded) override {
    if (is_discarded) {
      run_loop_->Quit();
    }
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
};

}  // namespace

class PerformanceInterventionInteractiveTest
    : public InteractiveFeaturePromoTest {
 public:
  PerformanceInterventionInteractiveTest()
      : InteractiveFeaturePromoTest(UseDefaultTrackerAllowingPromos(
            {feature_engagement::kIPHPerformanceInterventionDialogFeature})) {}
  ~PerformanceInterventionInteractiveTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    feature_list_.InitAndEnableFeatureWithParameters(
        performance_manager::features::kPerformanceInterventionUI,
        {{"intervention_show_mixed_profile", "false"},
         {"intervention_dialog_version", "2"}});
    InteractiveFeaturePromoTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveFeaturePromoTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Profile* CreateTestProfile() {
    ProfileManager* const profile_manager =
        g_browser_process->profile_manager();
    const base::FilePath new_path =
        profile_manager->GenerateNextProfileDirectoryPath();
    Profile* const profile =
        &profiles::testing::CreateProfileSync(profile_manager, new_path);
    auto* const tracker =
        feature_engagement::TrackerFactory::GetForBrowserContext(profile);
    base::RunLoop run_loop;
    tracker->AddOnInitializedCallback(base::BindOnce(
        [](base::OnceClosure callback, bool success) {
          ASSERT_TRUE(success);
          std::move(callback).Run();
        },
        run_loop.QuitClosure()));
    run_loop.Run();
    return profile;
  }

  GURL GetURL(std::string_view hostname = "example.com",
              std::string_view path = "/title1.html") {
    return embedded_test_server()->GetURL(hostname, path);
  }

  std::vector<resource_attribution::PageContext> GetPageContextForTabs(
      const std::vector<int>& tab_indices,
      Browser* browser) {
    std::vector<resource_attribution::PageContext> page_contexts;
    TabStripModel* const tab_strip_model = browser->tab_strip_model();
    for (int index : tab_indices) {
      content::WebContents* const web_contents =
          tab_strip_model->GetWebContentsAt(index);
      std::optional<resource_attribution::PageContext> context =
          resource_attribution::PageContext::FromWebContents(web_contents);
      CHECK(context.has_value());
      page_contexts.push_back(context.value());
    }

    return page_contexts;
  }

  void NotifyActionableTabListChange(const std::vector<int>& tab_indices,
                                     Browser* browser) {
    performance_manager::user_tuning::PerformanceDetectionManager::GetInstance()
        ->NotifyActionableTabObserversForTesting(
            PerformanceDetectionManager::ResourceType::kCpu,
            GetPageContextForTabs(tab_indices, browser));
  }

  auto TriggerOnActionableTabListChange(const std::vector<int>& tab_indices) {
    return Do([&]() { NotifyActionableTabListChange(tab_indices, browser()); });
  }

  auto CloseTab(int index) {
    return Do(base::BindLambdaForTesting([=, this]() {
      browser()->tab_strip_model()->CloseWebContentsAt(
          index, TabCloseTypes::CLOSE_NONE);
    }));
  }

  auto CheckTabDiscardStatus(int index, bool discarded) {
    return Check([=, this]() {
      TabStripModel* const tab_strip_model = browser()->tab_strip_model();
      return tab_strip_model->GetWebContentsAt(index)->WasDiscarded() ==
             discarded;
    });
  }

  auto SimulateFocusOnTextContainer(const ElementSpecifier& tab_row_id) {
    return WithView(tab_row_id, [](TabListRowView* tab_list_row) {
      tab_list_row->GetTextContainerForTesting()->RequestFocus();
      ASSERT_TRUE(tab_list_row->GetTextContainerForTesting()->HasFocus());
    });
  }

  auto SetShowNotificationPref(bool enabled) {
    return Do([=]() {
      PrefService* const pref_service = g_browser_process->local_state();
      pref_service->SetBoolean(performance_manager::user_tuning::prefs::
                                   kPerformanceInterventionNotificationEnabled,
                               enabled);
    });
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       ShowAndHideButton) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      // Flush the event queue to ensure that we trigger the button
      // to hide after it is shown.

      PressButton(kToolbarPerformanceInterventionButtonElementId),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      EnsurePresent(kToolbarPerformanceInterventionButtonElementId),
      TriggerOnActionableTabListChange({}),
      WaitForHide(kToolbarPerformanceInterventionButtonElementId));
}

IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       BodyTextControlledByFeatureParamSingluarTab) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      CheckViewProperty(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody,
          &views::Label::GetText,
          l10n_util::GetStringUTF16(
              IDS_PERFORMANCE_INTERVENTION_DIALOG_BODY_SINGULAR_V2)));
}

IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       BodyTextControlledByFeatureParamPluralTabs) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()),
      TriggerOnActionableTabListChange({0, 1}),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      CheckViewProperty(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody,
          &views::Label::GetText,
          l10n_util::GetStringUTF16(
              IDS_PERFORMANCE_INTERVENTION_DIALOG_BODY_V2)));
}

IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       LimitShowingButton) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      EnsureNotPresent(kToolbarPerformanceInterventionButtonElementId),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),

      PressButton(kToolbarPerformanceInterventionButtonElementId),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      // Flush the event queue to ensure that we trigger the button to hide
      // after it is shown.
      TriggerOnActionableTabListChange({}),
      WaitForHide(kToolbarPerformanceInterventionButtonElementId),
      TriggerOnActionableTabListChange({0}),
      EnsureNotPresent(kToolbarPerformanceInterventionButtonElementId));
}

// Making an actionable tab active should hide the intervention toolbar button
// because the actionable tab list is no longer valid.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       ActivateActionableTab) {
  RunTestSequence(
      InstrumentTab(kFirstTab, 0), AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()),
      EnsureNotPresent(kToolbarPerformanceInterventionButtonElementId),
      TriggerOnActionableTabListChange({0, 1}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),

      PressButton(kToolbarPerformanceInterventionButtonElementId),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      EnsurePresent(kToolbarPerformanceInterventionButtonElementId),
      // Flush the event queue to ensure that we trigger the button to hide
      // after it is shown.
      SelectTab(kTabStripElementId, 0), WaitForShow(kFirstTab),
      WaitForHide(kToolbarPerformanceInterventionButtonElementId));
}

// The intervention toolbar button should remain visible after closing an
// actionable tab is there are more tabs that are still actionable.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       CloseActionableTab) {
  RunTestSequence(
      InstrumentTab(kFirstTab, 0), AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()),
      EnsureNotPresent(kToolbarPerformanceInterventionButtonElementId),
      TriggerOnActionableTabListChange({0, 1}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      // Flush the event queue to ensure that we trigger the button to hide
      // after it is shown.
      CloseTab(1),
      // Button should still be showing since there is another actionable tab
      EnsurePresent(kToolbarPerformanceInterventionButtonElementId),
      CloseTab(0), WaitForHide(kToolbarPerformanceInterventionButtonElementId));
}

// Pixel test to verify that the performance intervention toolbar
// button looks correct.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       InterventionToolbarButton) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      // Flush the event queue to ensure that the screenshot happens
      // after the button is shown.

      PressButton(PerformanceInterventionBubble::
                      kPerformanceInterventionDialogDismissButton),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      SetOnIncompatibleAction(OnIncompatibleAction::kSkipTest,
                              kSkipPixelTestsReason),
      EnsurePresent(kToolbarPerformanceInterventionButtonElementId),
      Screenshot(kToolbarPerformanceInterventionButtonElementId,
                 /*screenshot_name=*/"InterventionToolbarButton",
                 /*baseline_cl=*/"5503223"));
}

// Dialog toggles between open and close when clicking on toolbar button
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       DialogRespondsToToolbarButtonClick) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),

      PressButton(kToolbarPerformanceInterventionButtonElementId),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),

      PressButton(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody));
}

// While the dialog is already visible, any changes to the actionable tab list
// should not affect the button and dialog visibility.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       DialogUnaffectedByActionableTabChange) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),

      // Triggering the actionable tab list again shouldn't affect
      // dialog visibility
      TriggerOnActionableTabListChange({0, 1}),
      EnsurePresent(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),

      // Dialog should stay open even though no tabs are actionable
      TriggerOnActionableTabListChange({}),
      EnsurePresent(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody));
}

// If the actionable tab list becomes empty while the intervention dialog is
// showing, after the dialog closes, the button should hide since there are no
// actionable tabs.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       ButtonHidesAfterDialogCloses) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),

      // Triggering the actionable tab list again shouldn't affect
      // dialog visibility
      TriggerOnActionableTabListChange({}),
      EnsurePresent(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),

      PressButton(PerformanceInterventionBubble::
                      kPerformanceInterventionDialogDismissButton),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      WaitForHide(kToolbarPerformanceInterventionButtonElementId));
}

// Clicking the dismiss dialog button should keep the toolbar button if the
// actionable tab list didn't become empty while the dialog was open.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       ButtonStaysAfterDismissClicked) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),

      PressButton(PerformanceInterventionBubble::
                      kPerformanceInterventionDialogDismissButton),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      EnsurePresent(kToolbarPerformanceInterventionButtonElementId));
}

// Clicking the deactivate dialog button should immediately hide the performance
// intervention toolbar button because the user enacted the suggested action.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       ButtonHidesAfterDeactivateClicked) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()),
      TriggerOnActionableTabListChange({0}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),

      PressButton(PerformanceInterventionBubble::
                      kPerformanceInterventionDialogDeactivateButton),
      WaitForHide(
          PerformanceInterventionBubble::kPerformanceInterventionDialogBody),
      WaitForHide(kToolbarPerformanceInterventionButtonElementId));
}

// The dialog should discard tabs suggested in the tab list
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       TakeSuggestedAction) {
  auto waiter = std::make_unique<DiscardWaiter>();
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()), SelectTab(kTabStripElementId, 0),
      TriggerOnActionableTabListChange({1, 2}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(PerformanceInterventionBubble::
                      kPerformanceInterventionDialogDeactivateButton),

      PressButton(PerformanceInterventionBubble::
                      kPerformanceInterventionDialogDeactivateButton),
      Do([&]() { waiter->Wait(); }), CheckTabDiscardStatus(0, false),
      CheckTabDiscardStatus(1, true), CheckTabDiscardStatus(2, true));
}

// The dialog should discard tabs suggested in the tab list
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       RemoveSuggestedTabFromList) {
  const char kTabListRow[] = "TabListRow";
  const char kSuggestedCloseButton[] = "SuggestedCloseButton";
  auto waiter = std::make_unique<DiscardWaiter>();

  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()), SelectTab(kTabStripElementId, 0),
      TriggerOnActionableTabListChange({1, 2}),
      WaitForShow(kToolbarPerformanceInterventionButtonElementId),
      WaitForShow(
          PerformanceInterventionBubble::kPerformanceInterventionTabList),

      NameViewRelative(
          PerformanceInterventionBubble::kPerformanceInterventionTabList,
          kTabListRow,
          [](TabListView* tab_list) {
            return views::AsViewClass<TabListRowView>(tab_list->children()[0]);
          }),
      SimulateFocusOnTextContainer(kTabListRow),
      CheckView(kTabListRow,
                [](TabListRowView* tab_list_row) {
                  return tab_list_row->GetCloseButtonForTesting()->GetVisible();
                }),
      NameViewRelative(kTabListRow, kSuggestedCloseButton,
                       [](TabListRowView* tab_list_row) {
                         return tab_list_row->GetCloseButtonForTesting();
                       }),
      PressButton(kSuggestedCloseButton), WaitForHide(kSuggestedCloseButton),
      PressButton(PerformanceInterventionBubble::
                      kPerformanceInterventionDialogDeactivateButton),
      Do([&]() { waiter->Wait(); }), CheckTabDiscardStatus(0, false),
      CheckTabDiscardStatus(1, false), CheckTabDiscardStatus(2, true));
}

// Intervention dialog should only show when the performance intervention
// notification pref is enabled
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       UiShowsWhenPrefEnabled) {
  RunTestSequence(
      AddInstrumentedTab(kSecondTab, GetURL()),
      AddInstrumentedTab(kThirdTab, GetURL()), SelectTab(kTabStripElementId, 0),
      SetShowNotificationPref(false), TriggerOnActionableTabListChange({1}),

      EnsureNotPresent(kToolbarPerformanceInterventionButtonElementId),
      SetShowNotificationPref(true), TriggerOnActionableTabListChange({1, 2}),

      WaitForShow(kToolbarPerformanceInterventionButtonElementId));
}

#if !(BUILDFLAG(IS_LINUX) && BUILDFLAG(IS_OZONE_WAYLAND))
// TODO(crbug.com/40863331): Linux Wayland doesn't support window activation
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       UiShowsOnlyOnActiveWindow) {
  // Create two browser windows with tabs and ensure the second browser window
  // is active
  Browser* const first_browser = browser();
  ASSERT_TRUE(AddTabAtIndexToBrowser(first_browser, 0, GetURL("a.com"),
                                     ui::PageTransition::PAGE_TRANSITION_LINK));
  ASSERT_TRUE(AddTabAtIndexToBrowser(first_browser, 1, GetURL("b.com"),
                                     ui::PageTransition::PAGE_TRANSITION_LINK));
  Browser* const second_browser = CreateBrowser(first_browser->profile());
  ASSERT_TRUE(AddTabAtIndexToBrowser(second_browser, 0, GetURL("c.com"),
                                     ui::PageTransition::PAGE_TRANSITION_LINK));
  BrowserWindow* const first_browser_window = first_browser->window();
  BrowserWindow* const second_browser_window = second_browser->window();
  second_browser_window->Activate();
  ASSERT_TRUE(second_browser_window->IsActive());
  ASSERT_FALSE(first_browser_window->IsActive());

  ToolbarButton* const first_button =
      BrowserView::GetBrowserViewForBrowser(first_browser)
          ->toolbar()
          ->performance_intervention_button();
  ToolbarButton* const second_button =
      BrowserView::GetBrowserViewForBrowser(second_browser)
          ->toolbar()
          ->performance_intervention_button();
  ASSERT_FALSE(first_button->GetVisible());
  ASSERT_FALSE(second_button->GetVisible());

  // Second browser window should show the performance intervention button since
  // it is the active browser.
  NotifyActionableTabListChange({0, 1}, first_browser);
  EXPECT_FALSE(first_button->GetVisible());
  EXPECT_TRUE(second_button->GetVisible());

  // Switching the active browser to the first browser and triggering the
  // performance detection manager shouldn't cause the UI to show on the first
  // browser since we already showed the notification for the day.
  ui_test_utils::BrowserActivationWaiter first_browser_waiter(first_browser);
  first_browser_window->Activate();
  first_browser_waiter.WaitForActivation();
  ASSERT_FALSE(second_browser_window->IsActive());
  ASSERT_TRUE(first_browser_window->IsActive());
  NotifyActionableTabListChange({0}, first_browser);
  EXPECT_FALSE(first_button->GetVisible());
  EXPECT_TRUE(second_button->GetVisible());
}

// The performance intervention toolbar button should hide when it is notified
// that there is no longer any actionable tabs even though the button is being
// shown on a non-active window.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       NonactiveInterventionButtonHides) {
  Browser* const first_browser = browser();
  ASSERT_TRUE(AddTabAtIndexToBrowser(first_browser, 0, GetURL("a.com"),
                                     ui::PageTransition::PAGE_TRANSITION_LINK));
  ASSERT_TRUE(AddTabAtIndexToBrowser(first_browser, 1, GetURL("b.com"),
                                     ui::PageTransition::PAGE_TRANSITION_LINK));
  Browser* const second_browser = CreateBrowser(first_browser->profile());
  ASSERT_TRUE(AddTabAtIndexToBrowser(second_browser, 0, GetURL("c.com"),
                                     ui::PageTransition::PAGE_TRANSITION_LINK));
  BrowserWindow* const first_browser_window = first_browser->window();
  BrowserWindow* const second_browser_window = second_browser->window();
  second_browser_window->Activate();
  ASSERT_TRUE(second_browser_window->IsActive());

  // Show the intervention button on the second browser window.
  NotifyActionableTabListChange({0, 1}, first_browser);
  PerformanceInterventionButton* const intervention_button =
      BrowserView::GetBrowserViewForBrowser(second_browser)
          ->toolbar()
          ->performance_intervention_button();
  EXPECT_TRUE(intervention_button->GetVisible());
  EXPECT_TRUE(intervention_button->IsBubbleShowing());

  // Dismiss the dialog.
  views::test::WidgetDestroyedWaiter widget_waiter(
      intervention_button->bubble_dialog_model_host()->GetWidget());
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi test_api(intervention_button);
  test_api.NotifyClick(e);
  widget_waiter.Wait();
  EXPECT_TRUE(intervention_button->GetVisible());
  EXPECT_FALSE(intervention_button->IsBubbleShowing());

  // Activate the first browser window.
  ui_test_utils::BrowserActivationWaiter first_browser_waiter(first_browser);
  first_browser_window->Activate();
  first_browser_waiter.WaitForActivation();
  ASSERT_FALSE(second_browser_window->IsActive());
  ASSERT_TRUE(first_browser_window->IsActive());
  EXPECT_TRUE(intervention_button->GetVisible());

  // Triggering a non-empty actionable tab list should keep the toolbar button
  // visible.
  NotifyActionableTabListChange({0}, first_browser);
  EXPECT_TRUE(intervention_button->GetVisible());
  EXPECT_FALSE(intervention_button->IsBubbleShowing());

  // Triggering an empty actionable tab list should immediately hide the
  // intervention button even though the button is in the non-active window.
  NotifyActionableTabListChange({}, first_browser);
  EXPECT_FALSE(intervention_button->GetVisible());
}
#endif

// We can only have one non-off record profile open at a time on ChromeOS so
// users will not encounter this case.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PerformanceInterventionInteractiveTest,
                       SuggestTabsOnlyForLastActiveProfile) {
  // Create two browser windows with tabs and ensure the second browser window
  // is active
  Browser* const first_browser = browser();
  ASSERT_TRUE(AddTabAtIndexToBrowser(first_browser, 0, GetURL("a.com"),
                                     ui::PageTransition::PAGE_TRANSITION_LINK));
  ASSERT_TRUE(AddTabAtIndexToBrowser(first_browser, 1, GetURL("b.com"),
                                     ui::PageTransition::PAGE_TRANSITION_LINK));

  Browser* const second_browser = CreateBrowser(CreateTestProfile());
  ASSERT_TRUE(AddTabAtIndexToBrowser(second_browser, 0, GetURL("c.com"),
                                     ui::PageTransition::PAGE_TRANSITION_LINK));
  BrowserWindow* const first_browser_window = first_browser->window();
  BrowserWindow* const second_browser_window = second_browser->window();
  second_browser_window->Activate();
  ASSERT_TRUE(second_browser_window->IsActive());
  ASSERT_FALSE(first_browser_window->IsActive());

  ToolbarButton* const first_button =
      BrowserView::GetBrowserViewForBrowser(first_browser)
          ->toolbar()
          ->performance_intervention_button();
  ToolbarButton* const second_button =
      BrowserView::GetBrowserViewForBrowser(second_browser)
          ->toolbar()
          ->performance_intervention_button();
  ASSERT_FALSE(first_button->GetVisible());
  ASSERT_FALSE(second_button->GetVisible());
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kMessageTriggerResultHistogram,
      InterventionMessageTriggerResult::kMixedProfile, 0);
  histogram_tester.ExpectBucketCount(kMessageTriggerResultHistogram,
                                     InterventionMessageTriggerResult::kShown,
                                     0);

  // The toolbar button should not show because the tabs on the first browser is
  // actionable but it is in a different profile from the last active profile
  NotifyActionableTabListChange({0, 1}, first_browser);
  EXPECT_FALSE(first_button->GetVisible());
  EXPECT_FALSE(second_button->GetVisible());
  histogram_tester.ExpectBucketCount(
      kMessageTriggerResultHistogram,
      InterventionMessageTriggerResult::kMixedProfile, 1);
  histogram_tester.ExpectBucketCount(kMessageTriggerResultHistogram,
                                     InterventionMessageTriggerResult::kShown,
                                     0);
}
#endif

class PerformanceInterventionNonUiMetricsTest
    : public PerformanceInterventionInteractiveTest {
 public:
  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    feature_list_.InitWithFeatures(
        {performance_manager::features::kPerformanceIntervention},
        {performance_manager::features::kPerformanceInterventionUI});
    InteractiveFeaturePromoTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/355466439): Fix test to work with UI after performance
// intervention rolls out.
IN_PROC_BROWSER_TEST_F(PerformanceInterventionNonUiMetricsTest,
                       TriggerMetricsRecorded) {
  base::HistogramTester histogram_tester;
  RunTestSequence(AddInstrumentedTab(kSecondTab, GetURL()),
                  AddInstrumentedTab(kThirdTab, GetURL()),
                  SelectTab(kTabStripElementId, 0), Do([&]() {
                    // verify that metrics were recorded
                    histogram_tester.ExpectBucketCount(
                        kMessageTriggerResultHistogram,
                        InterventionMessageTriggerResult::kShown, 0);
                    histogram_tester.ExpectBucketCount(
                        kMessageTriggerResultHistogram,
                        InterventionMessageTriggerResult::kRateLimited, 0);
                  }),
                  TriggerOnActionableTabListChange({1, 2}), Do([&]() {
                    // verify that metrics were recorded
                    histogram_tester.ExpectBucketCount(
                        kMessageTriggerResultHistogram,
                        InterventionMessageTriggerResult::kShown, 1);
                    histogram_tester.ExpectBucketCount(
                        kMessageTriggerResultHistogram,
                        InterventionMessageTriggerResult::kRateLimited, 0);
                  }),
                  TriggerOnActionableTabListChange({1}), Do([&]() {
                    // verify that metrics were recorded
                    histogram_tester.ExpectBucketCount(
                        kMessageTriggerResultHistogram,
                        InterventionMessageTriggerResult::kShown, 1);
                    histogram_tester.ExpectBucketCount(
                        kMessageTriggerResultHistogram,
                        InterventionMessageTriggerResult::kRateLimited, 1);
                  }));
}

class PerformanceInterventionMixedProfileTest
    : public PerformanceInterventionInteractiveTest {
 public:
  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);

    feature_list_.InitAndEnableFeatureWithParameters(
        performance_manager::features::kPerformanceInterventionUI,
        {{"intervention_show_mixed_profile", "true"}});
    InteractiveFeaturePromoTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// We can only have one non-off record profile open at a time on ChromeOS so
// users will not encounter this case.
#if !BUILDFLAG(IS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(PerformanceInterventionMixedProfileTest,
                       SuggestTabsForMultipleProfiles) {
  // Create two browser windows with tabs and ensure the second browser window
  // is active
  Browser* const first_browser = browser();
  ASSERT_TRUE(AddTabAtIndexToBrowser(first_browser, 0, GetURL("a.com"),
                                     ui::PageTransition::PAGE_TRANSITION_LINK));
  ASSERT_TRUE(AddTabAtIndexToBrowser(first_browser, 1, GetURL("b.com"),
                                     ui::PageTransition::PAGE_TRANSITION_LINK));

  Browser* const second_browser = CreateBrowser(CreateTestProfile());
  ASSERT_TRUE(AddTabAtIndexToBrowser(second_browser, 0, GetURL("c.com"),
                                     ui::PageTransition::PAGE_TRANSITION_LINK));
  BrowserWindow* const first_browser_window = first_browser->window();
  BrowserWindow* const second_browser_window = second_browser->window();
  second_browser_window->Activate();
  ASSERT_TRUE(second_browser_window->IsActive());
  ASSERT_FALSE(first_browser_window->IsActive());

  ToolbarButton* const first_button =
      BrowserView::GetBrowserViewForBrowser(first_browser)
          ->toolbar()
          ->performance_intervention_button();
  ToolbarButton* const second_button =
      BrowserView::GetBrowserViewForBrowser(second_browser)
          ->toolbar()
          ->performance_intervention_button();
  ASSERT_FALSE(first_button->GetVisible());
  ASSERT_FALSE(second_button->GetVisible());

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kMessageTriggerResultHistogram,
      InterventionMessageTriggerResult::kMixedProfile, 0);
  histogram_tester.ExpectBucketCount(kMessageTriggerResultHistogram,
                                     InterventionMessageTriggerResult::kShown,
                                     0);

  // We should show the toolbar button on the second browser because it is the
  // last active browser even though all actionable tabs belong to the first
  // browser.
  NotifyActionableTabListChange({0, 1}, first_browser);
  EXPECT_FALSE(first_button->GetVisible());
  EXPECT_TRUE(second_button->GetVisible());
  histogram_tester.ExpectBucketCount(
      kMessageTriggerResultHistogram,
      InterventionMessageTriggerResult::kMixedProfile, 0);
  histogram_tester.ExpectBucketCount(kMessageTriggerResultHistogram,
                                     InterventionMessageTriggerResult::kShown,
                                     1);
}
#endif
