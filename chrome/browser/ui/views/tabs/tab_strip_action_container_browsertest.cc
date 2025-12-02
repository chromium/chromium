// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"

#include "base/feature_list.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_policy_checker.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/states/actor_task_nudge_state.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/glic_actor_task_icon.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/animation/slide_animation.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/actor/resources/grit/actor_browser_resources.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/ui/tabs/glic_actor_nudge_controller.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_controller.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager_factory.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

namespace {
using base::test::RunUntil;
using testing::SizeIs;

using ActorTaskNudgeState = actor::ui::ActorTaskNudgeState;

}  // namespace

class TabStripActionContainerBrowserTest : public InProcessBrowserTest {
 public:
  TabStripActionContainerBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        {
            {features::kTabOrganization, {}},
#if BUILDFLAG(ENABLE_GLIC)
            {features::kGlicRollout, {}},
            {features::kGlicFreWarming, {}},
            {features::kGlicActor, {}},
            {features::kGlicActorUi,
             { {features::kGlicActorUiTaskIconName, "true"} }},
            {features::kGlicActorUiNudgeRedesign, {}},
#endif  // BUILDFLAG(ENABLE_GLIC)
            {features::kTabstripComboButton, {}},
            {features::kTabstripDeclutter, {}},
            {contextual_cueing::kContextualCueing, {}},
        },
        {});
    TabOrganizationUtils::GetInstance()->SetIgnoreOptGuideForTesting(true);
  }

#if BUILDFLAG(ENABLE_GLIC)
  void SetUp() override {
    // This will temporarily disable preloading.
    glic::GlicProfileManager::ForceMemoryPressureForTesting(
        base::MEMORY_PRESSURE_LEVEL_CRITICAL);
    fre_server_.ServeFilesFromDirectory(
        base::PathService::CheckedGet(base::DIR_ASSETS)
            .AppendASCII("gen/chrome/test/data/webui/glic/"));
    ASSERT_TRUE(fre_server_.Start());
    fre_url_ = fre_server_.GetURL("/glic/test_client/fre.html");

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kGlicFreURL, fre_url_.spec());
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    glic::GlicProfileManager::ForceMemoryPressureForTesting(std::nullopt);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }
#endif  // BUILDFLAG(ENABLE_GLIC)

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabStripActionContainer* tab_strip_action_container() {
    return BrowserElementsViews::From(browser())
        ->GetViewAs<TabStripActionContainer>(kTabStripActionContainerElementId);
  }

 protected:
  TabStripNudgeButton* TabDeclutterButton() {
    return tab_strip_action_container()->tab_declutter_button();
  }
  TabStripNudgeButton* AutoTabGroupButton() {
    return tab_strip_action_container()->auto_tab_group_button();
  }

  glic::GlicButton* GlicNudgeButton() {
    return tab_strip_action_container()->GetGlicButton();
  }

  glic::GlicActorTaskIcon* GlicActorTaskIcon() {
    return tab_strip_action_container()->glic_actor_task_icon();
  }

  views::FlexLayoutView* GlicActorButtonContainer() {
    return tab_strip_action_container()->glic_actor_button_container();
  }

  void ShowTabStripNudgeButton(TabStripNudgeButton* button) {
    tab_strip_action_container()->ShowTabStripNudge(button);
  }
  void HideTabStripNudgeButton(TabStripNudgeButton* button) {
    tab_strip_action_container()->HideTabStripNudge(button);
  }

  void OnTabStripNudgeButtonTimeout(TabStripNudgeButton* button) {
    tab_strip_action_container()->OnTabStripNudgeButtonTimeout(button);
  }

  gfx::SlideAnimation* GetExpansionAnimation(TabStripNudgeButton* button) {
    if (tab_strip_action_container()->ButtonOwnsAnimation(button)) {
      return button->GetExpansionAnimationForTesting();
    }
    return tab_strip_action_container()
        ->animation_session_for_testing()
        ->expansion_animation();
  }

  void SetLockedExpansionMode(LockedExpansionMode mode,
                              TabStripNudgeButton* button) {
    tab_strip_action_container()->SetLockedExpansionMode(mode, button);
  }
  void OnButtonClicked(TabStripNudgeButton* button) {
    if (button == TabDeclutterButton()) {
      tab_strip_action_container()->OnTabDeclutterButtonClicked();
    } else if (button == AutoTabGroupButton()) {
      tab_strip_action_container()->OnAutoTabGroupButtonClicked();
    } else if (button == GlicNudgeButton()) {
#if BUILDFLAG(ENABLE_GLIC)
      tab_strip_action_container()->OnGlicButtonClicked();
#else
      NOTREACHED();
#endif  // BUILDFLAG(ENABLE_GLIC)
    } else if (button == GlicActorTaskIcon()) {
#if BUILDFLAG(ENABLE_GLIC)
      tab_strip_action_container()->OnGlicActorTaskIconClicked();
#else
      NOTREACHED();
#endif  // BUILDFLAG(ENABLE_GLIC)
    }
  }
  void OnButtonDismissed(TabStripNudgeButton* button) {
    if (button == TabDeclutterButton()) {
      tab_strip_action_container()->OnTabDeclutterButtonDismissed();
    } else if (button == AutoTabGroupButton()) {
      tab_strip_action_container()->OnAutoTabGroupButtonDismissed();
    } else if (button == GlicNudgeButton()) {
#if BUILDFLAG(ENABLE_GLIC)
      tab_strip_action_container()->OnGlicButtonDismissed();
#else
      NOTREACHED();
#endif  // BUILDFLAG(ENABLE_GLIC)
    }
  }

#if BUILDFLAG(ENABLE_GLIC)
  void ResetMemoryPressure() {
    glic::GlicProfileManager::ForceMemoryPressureForTesting(
        base::MEMORY_PRESSURE_LEVEL_NONE);
  }

  const GURL& fre_url() { return fre_url_; }
#endif  // BUILDFLAG(ENABLE_GLIC)
  void ResetAnimation(int value) {
    if (tab_strip_action_container()->animation_session_for_testing()) {
      tab_strip_action_container()
          ->animation_session_for_testing()
          ->ResetOpacityAnimationForTesting(value);
      if (tab_strip_action_container()->animation_session_for_testing()) {
        tab_strip_action_container()
            ->animation_session_for_testing()
            ->ResetExpansionAnimationForTesting(value);
      }
    }
  }

  void Click(views::View* clickable_view) {
    clickable_view->OnMousePressed(
        ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
    clickable_view->OnMouseReleased(ui::MouseEvent(
        ui::EventType::kMouseReleased, gfx::Point(), gfx::Point(),
        ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0));
  }

 protected:
#if BUILDFLAG(ENABLE_GLIC)
  glic::GlicTestEnvironment glic_test_environment_;
  net::EmbeddedTestServer fre_server_;
  GURL fre_url_;
#endif  // BUILDFLAG(ENABLE_GLIC)
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest, ShowsDeclutterChip) {
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());

  ShowTabStripNudgeButton(TabDeclutterButton());

  ASSERT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       ShowsAndHidesDeclutterChip) {
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());

  ShowTabStripNudgeButton(TabDeclutterButton());

  ASSERT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());

  // Finish showing declutter chip.
  ResetAnimation(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  // Hide the declutter chip.
  HideTabStripNudgeButton(TabDeclutterButton());

  ASSERT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       LogsWhenDeclutterButtonClicked) {
  base::HistogramTester histogram_tester;

  ShowTabStripNudgeButton(TabDeclutterButton());

  // Finish showing declutter chip.
  ResetAnimation(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  OnButtonClicked(TabDeclutterButton());

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Declutter.Trigger.Outcome", 0, 1);
  // Bucketed CTR metric should reflect one show and one click, with fewer than
  // 15 total tabs.
  histogram_tester.ExpectBucketCount(
      "Tab.Organization.Declutter.Trigger.BucketedCTR", 0, 1);
  histogram_tester.ExpectBucketCount(
      "Tab.Organization.Declutter.Trigger.BucketedCTR", 10, 1);
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       LogsWhenDeclutterButtonDismissed) {
  base::HistogramTester histogram_tester;

  ShowTabStripNudgeButton(TabDeclutterButton());

  // Finish showing declutter chip.
  ResetAnimation(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  OnButtonDismissed(TabDeclutterButton());

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Declutter.Trigger.Outcome", 1, 1);
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       LogsWhenDeclutterButtonTimeout) {
  base::HistogramTester histogram_tester;

  ShowTabStripNudgeButton(TabDeclutterButton());

  // Finish showing declutter chip.
  ResetAnimation(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  OnTabStripNudgeButtonTimeout(TabDeclutterButton());

  histogram_tester.ExpectUniqueSample(
      "Tab.Organization.Declutter.Trigger.Outcome", 2, 1);
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       LogsSuccessWhenAutoTabGroupsButtonClicked) {
  base::HistogramTester histogram_tester;

  ShowTabStripNudgeButton(AutoTabGroupButton());

  ResetAnimation(1);

  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  TabOrganizationService* service =
      tab_strip_action_container()->tab_organization_service_for_testing();

  service->OnTriggerOccured(browser());

  OnButtonClicked(AutoTabGroupButton());

  histogram_tester.ExpectUniqueSample("Tab.Organization.AllEntrypoints.Clicked",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.Proactive.Clicked",
                                      true, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.Trigger.Outcome", 0, 1);
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       LogsFailureWhenAutoTabGroupsButtonDismissed) {
  base::HistogramTester histogram_tester;

  ShowTabStripNudgeButton(AutoTabGroupButton());

  ResetAnimation(1);

  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  TabOrganizationService* service =
      tab_strip_action_container()->tab_organization_service_for_testing();

  service->OnTriggerOccured(browser());

  OnButtonDismissed(AutoTabGroupButton());

  histogram_tester.ExpectUniqueSample("Tab.Organization.Proactive.Clicked",
                                      false, 1);
  histogram_tester.ExpectUniqueSample("Tab.Organization.Trigger.Outcome", 1, 1);
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest, DelaysShow) {
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());

  SetLockedExpansionMode(LockedExpansionMode::kWillShow, TabDeclutterButton());

  ShowTabStripNudgeButton(TabDeclutterButton());

  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());

  SetLockedExpansionMode(LockedExpansionMode::kNone, nullptr);

  ASSERT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest, DelaysHide) {
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());

  ShowTabStripNudgeButton(TabDeclutterButton());

  ResetAnimation(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());

  SetLockedExpansionMode(LockedExpansionMode::kWillHide, TabDeclutterButton());

  HideTabStripNudgeButton(TabDeclutterButton());

  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());

  SetLockedExpansionMode(LockedExpansionMode::kNone, nullptr);

  ASSERT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       ImmediatelyHidesWhenOrganizeButtonClicked) {
  ShowTabStripNudgeButton(TabDeclutterButton());
  ResetAnimation(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  SetLockedExpansionMode(LockedExpansionMode::kWillHide, TabDeclutterButton());

  OnButtonClicked(TabDeclutterButton());

  EXPECT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       ImmediatelyHidesWhenOrganizeButtonDismissed) {
  ShowTabStripNudgeButton(TabDeclutterButton());
  ResetAnimation(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  SetLockedExpansionMode(LockedExpansionMode::kWillHide, TabDeclutterButton());

  OnButtonDismissed(TabDeclutterButton());

  EXPECT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}

#if BUILDFLAG(ENABLE_GLIC)
IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       ImmediatelyHidesWhenGlicNudgeButtonDismissed) {
  ShowTabStripNudgeButton(GlicNudgeButton());
  GetExpansionAnimation(GlicNudgeButton())->Reset(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  SetLockedExpansionMode(LockedExpansionMode::kWillHide, GlicNudgeButton());

  OnButtonDismissed(GlicNudgeButton());
  EXPECT_TRUE(GetExpansionAnimation(GlicNudgeButton())->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       LogsWhenGlicNudgeButtonClicked) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  ShowTabStripNudgeButton(GlicNudgeButton());

  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  OnButtonClicked(GlicNudgeButton());
  auto* const glic_keyed_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(
          browser()->GetProfile());

  EXPECT_TRUE(glic_keyed_service->IsWindowShowing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest, PreloadFreOnNudge) {
  // We set an artificial activity callback here because it is required for
  // OnTriggerGlicNudgeUI to actually show the nudge.
  auto* nudge_controller =
      browser()->browser_window_features()->glic_nudge_controller();
  nudge_controller->SetNudgeActivityCallbackForTesting();

  auto* service = glic::GlicKeyedServiceFactory::GetGlicKeyedService(
      browser()->GetProfile());
  glic::SetFRECompletion(browser()->profile(),
                         glic::prefs::FreStatus::kNotStarted);
  EXPECT_TRUE(service->fre_controller().ShouldShowFreDialog());
  EXPECT_FALSE(service->fre_controller().IsWarmed());

  // This will enable preloading again.
  ResetMemoryPressure();

  base::RunLoop run_loop;
  auto subscription = service->fre_controller().AddWebUiStateChangedCallback(
      base::BindRepeating(
          [](base::RunLoop* run_loop, glic::mojom::FreWebUiState new_state) {
            if (new_state == glic::mojom::FreWebUiState::kReady) {
              run_loop->Quit();
            }
          },
          base::Unretained(&run_loop)));

  nudge_controller->UpdateNudgeLabel(
      browser()->tab_strip_model()->GetActiveWebContents(), "test",
      /*prompt_suggestion=*/std::nullopt, /*activity=*/std::nullopt,
      base::DoNothing());

  ShowTabStripNudgeButton(GlicNudgeButton());

  // Wait for the FRE to preload.
  run_loop.Run();
  EXPECT_TRUE(service->fre_controller().IsWarmed());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       ShowAndHideGlicButtonWhenGlicNudgeButtonShows) {
  ShowTabStripNudgeButton(GlicNudgeButton());
  GetExpansionAnimation(GlicNudgeButton())->Reset(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(1, tab_strip_action_container()
                   ->GetGlicButton()
                   ->width_factor_for_testing());
  SetLockedExpansionMode(LockedExpansionMode::kWillHide, GlicNudgeButton());

  OnButtonDismissed(GlicNudgeButton());

  GetExpansionAnimation(GlicNudgeButton())->Reset(0);
  EXPECT_EQ(0, tab_strip_action_container()
                   ->GetGlicButton()
                   ->width_factor_for_testing());
}

IN_PROC_BROWSER_TEST_F(
    TabStripActionContainerBrowserTest,
    ActivatesTabAndRemoveRowOnGlicActorTaskListBubbleRowClick) {
  ASSERT_TRUE(embedded_https_test_server().Start());
  auto* actor_service = actor::ActorKeyedService::Get(browser()->GetProfile());
  actor_service->GetPolicyChecker().SetActOnWebForTesting(true);
  actor::TaskId task_id = actor_service->CreateTask();
  actor::ActorTask* task = actor_service->GetTask(task_id);
  actor::ui::StartTask start_task_event(task_id);
  actor_service->GetActorUiStateManager()->OnUiEvent(start_task_event);

  // Navigate the active tab to a new page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL("/actor/blank.html")));

  // Add tab to task.
  base::test::TestFuture<actor::mojom::ActionResultPtr> add_tab_future;
  task->AddTab(browser()->GetActiveTabInterface()->GetHandle(),
               add_tab_future.GetCallback());
  auto add_tab_result = add_tab_future.Take();
  ASSERT_TRUE(add_tab_result);

  // Add and activate the non-actuation tab.
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1,
                                     GURL(chrome::kChromeUINewTabURL),
                                     ui::PAGE_TRANSITION_LINK));
  browser()->GetTabStripModel()->ActivateTabAt(1);

  actor_service->GetTask(task_id)->Pause(/*from_actor=*/true);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return GlicActorTaskIcon()->GetIsShowingNudge(); }));

  ResetAnimation(1);

  auto* bubble_controller = ActorTaskListBubbleController::From(browser());
  auto* content_view = bubble_controller->GetBubbleWidget()
                           ->widget_delegate()
                           ->AsBubbleDialogDelegate()
                           ->GetContentsView();
  EXPECT_EQ(1u, content_view->children().size());
  auto* button = static_cast<RichHoverButton*>(
      content_view->children().front()->children().front());
  Click(button);

  // Nudge should hide and row list should be emptied.
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return !GlicActorTaskIcon()->GetIsShowingNudge(); }));
  auto* manager = tabs::GlicActorTaskIconManagerFactory::GetForProfile(
      browser()->GetProfile());
  EXPECT_EQ(0u, manager->GetActorTaskListBubbleRows().size());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       OnlyExpandGlicIfNotExpanded) {
  // Show the nudge and finish the expansion animation.
  ShowTabStripNudgeButton(GlicNudgeButton());
  GetExpansionAnimation(GlicNudgeButton())->Reset(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_EQ(1, tab_strip_action_container()
                   ->GetGlicButton()
                   ->width_factor_for_testing());

  // Show again. Since we're already showing, the button should remain expanded.
  ShowTabStripNudgeButton(GlicNudgeButton());
  EXPECT_EQ(1, tab_strip_action_container()
                   ->GetGlicButton()
                   ->width_factor_for_testing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       OnlyCollapseGlicIfNotCollapsed) {
  // Show the nudge and finish the expansion animation.
  ShowTabStripNudgeButton(GlicNudgeButton());
  GetExpansionAnimation(GlicNudgeButton())->Reset(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_EQ(1, tab_strip_action_container()
                   ->GetGlicButton()
                   ->width_factor_for_testing());

  // Collapse.
  SetLockedExpansionMode(LockedExpansionMode::kWillHide, GlicNudgeButton());
  OnButtonDismissed(GlicNudgeButton());
  GetExpansionAnimation(GlicNudgeButton())->Reset(0);
  EXPECT_EQ(0, tab_strip_action_container()
                   ->GetGlicButton()
                   ->width_factor_for_testing());

  // Collapse again. The button should remain collapsed.
  OnButtonDismissed(GlicNudgeButton());
  EXPECT_EQ(0, tab_strip_action_container()
                   ->GetGlicButton()
                   ->width_factor_for_testing());
}

// TODO(crbug.com/451697169): Fix this test for Windows and Linux.
// TODO(crbug.com/461145884): Enable on ChromeOS
// TODO(crbug.com/465247286): Fix this for Mac.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_MAC)
#define MAYBE_GlicLabelEnablementFollowsWindowActivation \
  DISABLED_GlicLabelEnablementFollowsWindowActivation
#else
#define MAYBE_GlicLabelEnablementFollowsWindowActivation \
  GlicLabelEnablementFollowsWindowActivation
#endif
IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       MAYBE_GlicLabelEnablementFollowsWindowActivation) {
  tab_strip_action_container()->GetWidget()->Activate();
  EXPECT_TRUE(tab_strip_action_container()
                  ->GetGlicButton()
                  ->GetLabelEnabledForTesting());

  // Create/activate a different widget (just calling Deactivate() on the
  // browser window isn't enough, since it will have no effect if there isn't
  // another window to become active.)
  auto widget_2 = std::make_unique<views::Widget>(
      views::Widget::InitParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                                views::Widget::InitParams::TYPE_WINDOW));
  widget_2->Activate();
  EXPECT_FALSE(tab_strip_action_container()
                   ->GetGlicButton()
                   ->GetLabelEnabledForTesting());

  // Activate the browser. The button label should be enabled again.
  tab_strip_action_container()->GetWidget()->Activate();
  EXPECT_TRUE(tab_strip_action_container()
                  ->GetGlicButton()
                  ->GetLabelEnabledForTesting());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       LogsWhenGlicActorTaskNudgeClicked) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(GlicActorButtonContainer()->GetVisible());
  ASSERT_THAT(GlicActorButtonContainer()->children(), SizeIs(1));

  auto* actor_service = actor::ActorKeyedService::Get(browser()->GetProfile());
  actor_service->GetPolicyChecker().SetActOnWebForTesting(true);
  actor::TaskId task_id = actor_service->CreateTask();
  actor::ActorTask* task = actor_service->GetTask(task_id);

  auto* manager = tabs::GlicActorTaskIconManagerFactory::GetForProfile(
      browser()->GetProfile());

  task->SetState(actor::ActorTask::State::kActing);
  task->Interrupt();
  manager->UpdateTaskNudge();

  EXPECT_TRUE(RunUntil([&]() { return GlicActorTaskIcon()->GetVisible(); }));
  EXPECT_TRUE(GlicActorButtonContainer()->GetVisible());
  EXPECT_TRUE(GlicActorTaskIcon()->GetIsShowingNudge());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Actor.Ui.TaskNudge.Shown",
               ActorTaskNudgeState::Text::kNeedsAttention) == 1;
  }));

  base::UserActionTester user_action_tester;
  OnButtonClicked(GlicActorTaskIcon());
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Actor.Ui.TaskNudge.NeedsAttention.Click"));
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       GlicActorCompleteDoesNotShowTaskNudge) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(GlicActorTaskIcon()->GetText(), std::u16string());
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());
  EXPECT_FALSE(GlicActorButtonContainer()->GetVisible());

  auto* actor_nudge_controller =
      tabs::GlicActorNudgeController::From(browser());
  auto actor_task_nudge_state = ActorTaskNudgeState();
  actor_task_nudge_state.text = ActorTaskNudgeState::Text::kCompleteTasks;
  actor_nudge_controller->OnStateUpdate(actor_task_nudge_state);

  EXPECT_EQ(GlicActorTaskIcon()->GetText(), std::u16string());
  EXPECT_FALSE(GlicActorButtonContainer()->GetVisible());
  EXPECT_FALSE(GlicActorTaskIcon()->GetIsShowingNudge());

  EXPECT_TRUE(RunUntil([&]() { return !GlicActorTaskIcon()->GetVisible(); }));
  EXPECT_EQ(histogram_tester.GetBucketCount(
                "Actor.Ui.TaskNudge.Shown",
                ActorTaskNudgeState::Text::kCompleteTasks),
            0);
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       ShowAndHideGlicActorCheckTasksNudge) {
  EXPECT_EQ(GlicActorTaskIcon()->GetText(), std::u16string());
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());
  EXPECT_FALSE(GlicActorButtonContainer()->GetVisible());

  auto* actor_nudge_controller =
      tabs::GlicActorNudgeController::From(browser());
  auto actor_task_nudge_state = ActorTaskNudgeState();
  actor_task_nudge_state.text = ActorTaskNudgeState::Text::kNeedsAttention;
  actor_nudge_controller->OnStateUpdate(actor_task_nudge_state);

  ASSERT_TRUE(RunUntil([&]() {
    return tab_strip_action_container()
        ->animation_session_for_testing()
        ->expansion_animation()
        ->IsShowing();
  }));
  EXPECT_TRUE(RunUntil([&]() { return GlicActorTaskIcon()->GetVisible(); }));
  EXPECT_TRUE(GlicActorButtonContainer()->GetVisible());
  EXPECT_TRUE(GlicActorTaskIcon()->GetIsShowingNudge());
  EXPECT_EQ(GlicActorTaskIcon()->GetText(),
            l10n_util::GetStringUTF16(IDR_ACTOR_CHECK_TASK_NUDGE_LABEL));

  ResetAnimation(1);

  actor_task_nudge_state.text = ActorTaskNudgeState::Text::kDefault;
  actor_nudge_controller->OnStateUpdate(actor_task_nudge_state);

  EXPECT_TRUE(RunUntil([&]() { return !GlicActorTaskIcon()->GetVisible(); }));
  EXPECT_EQ(GlicActorTaskIcon()->GetText(), std::u16string());
  EXPECT_FALSE(GlicActorButtonContainer()->GetVisible());
  EXPECT_FALSE(GlicActorTaskIcon()->GetIsShowingNudge());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       ResetGlicActorTaskNudgeOnCheckTaskToActiveStateChange) {
  base::HistogramTester histogram_tester;
  auto* actor_nudge_controller =
      tabs::GlicActorNudgeController::From(browser());
  auto actor_task_nudge_state = ActorTaskNudgeState();
  actor_task_nudge_state.text = ActorTaskNudgeState::Text::kNeedsAttention;
  actor_nudge_controller->OnStateUpdate(actor_task_nudge_state);

  ASSERT_TRUE(RunUntil([&]() {
    return tab_strip_action_container()
        ->animation_session_for_testing()
        ->expansion_animation()
        ->IsShowing();
  }));

  EXPECT_TRUE(RunUntil([&]() { return GlicActorTaskIcon()->GetVisible(); }));
  EXPECT_TRUE(GlicActorButtonContainer()->GetVisible());
  EXPECT_TRUE(GlicActorTaskIcon()->GetIsShowingNudge());
  EXPECT_EQ(GlicActorTaskIcon()->GetText(),
            l10n_util::GetStringUTF16(IDR_ACTOR_CHECK_TASK_NUDGE_LABEL));

  ResetAnimation(1);

  actor_task_nudge_state.text = ActorTaskNudgeState::Text::kDefault;
  actor_nudge_controller->OnStateUpdate(actor_task_nudge_state);

  EXPECT_TRUE(RunUntil([&]() { return !GlicActorTaskIcon()->GetVisible(); }));
  EXPECT_FALSE(GlicActorButtonContainer()->GetVisible());
  // Ensure the shown histogram was not recorded as the default state doesn't
  // show a nudge.
  EXPECT_EQ(
      histogram_tester.GetBucketCount("Actor.Ui.TaskNudge.Shown",
                                      ActorTaskNudgeState::Text::kDefault),
      0);
  // Check that GlicButton was removed from the GlicActorButtonContainer.
  ASSERT_THAT(GlicActorButtonContainer()->children(), SizeIs(1));
  EXPECT_EQ(GlicActorTaskIcon(), GlicActorButtonContainer()->children()[0]);
  EXPECT_EQ(GlicActorTaskIcon()->GetText(), std::u16string());
  EXPECT_FALSE(GlicActorTaskIcon()->GetIsShowingNudge());
}

IN_PROC_BROWSER_TEST_F(
    TabStripActionContainerBrowserTest,
    GlicActorNudgeDoesNotRetriggerOnSingleTaskNeedsAttentionToMultipleTextChange) {
  EXPECT_EQ(GlicActorTaskIcon()->GetText(), std::u16string());
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());
  EXPECT_FALSE(GlicActorButtonContainer()->GetVisible());

  auto* actor_nudge_controller =
      tabs::GlicActorNudgeController::From(browser());
  auto actor_task_nudge_state = ActorTaskNudgeState();
  actor_task_nudge_state.text = ActorTaskNudgeState::Text::kNeedsAttention;
  actor_nudge_controller->OnStateUpdate(actor_task_nudge_state);

  ASSERT_TRUE(RunUntil([&]() {
    return tab_strip_action_container()
        ->animation_session_for_testing()
        ->expansion_animation()
        ->IsShowing();
  }));

  EXPECT_TRUE(RunUntil([&]() { return GlicActorTaskIcon()->GetVisible(); }));
  EXPECT_TRUE(GlicActorButtonContainer()->GetVisible());
  EXPECT_TRUE(GlicActorTaskIcon()->GetIsShowingNudge());
  EXPECT_EQ(GlicActorTaskIcon()->GetText(),
            l10n_util::GetStringUTF16(IDR_ACTOR_CHECK_TASK_NUDGE_LABEL));

  ResetAnimation(1);

  actor_task_nudge_state.text =
      ActorTaskNudgeState::Text::kMultipleTasksNeedAttention;
  actor_nudge_controller->OnStateUpdate(actor_task_nudge_state);

  EXPECT_TRUE(RunUntil([&]() { return GlicActorTaskIcon()->GetVisible(); }));
  EXPECT_TRUE(GlicActorTaskIcon()->GetIsShowingNudge());
  // TODO(crbug.com/431015299): Replace with finalized strings when ready.
  EXPECT_EQ(GlicActorTaskIcon()->GetText(),
            actor_nudge_controller->GetCheckTasksNudgeLabel());
}

class TabStripActionContainerPreRedesignBrowserTest
    : public TabStripActionContainerBrowserTest {
 public:
  TabStripActionContainerPreRedesignBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {
            {features::kTabOrganization, {}},
#if BUILDFLAG(ENABLE_GLIC)
            {features::kGlicRollout, {}},
            {features::kGlicFreWarming, {}},
            {features::kGlicActor, {}},
            {features::kGlicActorUi,
             { {features::kGlicActorUiTaskIconName, "true"} }},
            {features::kGlicActorUiNudgeRedesign, {}},
#endif  // BUILDFLAG(ENABLE_GLIC)
            {features::kTabstripComboButton, {}},
            {features::kTabstripDeclutter, {}},
            {contextual_cueing::kContextualCueing, {}},
        },
#if BUILDFLAG(ENABLE_GLIC)
        /*disabled_features=*/{ features::kGlicActorUiNudgeRedesign }
#else
        /*disabled_features=*/{}
#endif  // BUILDFLAG(ENABLE_GLIC)
    );
    TabOrganizationUtils::GetInstance()->SetIgnoreOptGuideForTesting(true);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabStripActionContainerPreRedesignBrowserTest,
                       ShowAndHideGlicActorTaskIconCheckTasksNudge) {
  EXPECT_EQ(GlicActorTaskIcon()->GetText(), std::u16string());
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());
  EXPECT_FALSE(GlicActorButtonContainer()->GetVisible());

  auto* task_icon_controller =
      tabs::GlicActorTaskIconController::From(browser());
  tabs::ActorTaskIconState actor_task_icon_state = {
      .is_visible = true,
      .text = tabs::ActorTaskIconState::Text::kNeedsAttention};

  task_icon_controller->OnStateUpdate(
      /*is_showing=*/false, glic::mojom::CurrentView::kConversation,
      actor_task_icon_state);

  ASSERT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());

  EXPECT_TRUE(GlicActorButtonContainer()->GetVisible());
  EXPECT_TRUE(GlicActorTaskIcon()->GetVisible());
  EXPECT_TRUE(GlicActorTaskIcon()->GetIsShowingNudge());
  EXPECT_EQ(GlicActorTaskIcon()->GetText(),
            l10n_util::GetStringUTF16(IDR_ACTOR_CHECK_TASK_NUDGE_LABEL));

  ResetAnimation(1);

  actor_task_icon_state.is_visible = false;

  task_icon_controller->OnStateUpdate(
      /*is_showing=*/false, glic::mojom::CurrentView::kConversation,
      actor_task_icon_state);

  EXPECT_EQ(GlicActorTaskIcon()->GetText(), std::u16string());
  EXPECT_FALSE(GlicActorButtonContainer()->GetVisible());
  EXPECT_FALSE(GlicActorTaskIcon()->GetIsShowingNudge());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerPreRedesignBrowserTest,
                       ResetTaskIconOnCheckTaskToActiveStateChange) {
  auto* task_icon_controller =
      tabs::GlicActorTaskIconController::From(browser());
  tabs::ActorTaskIconState actor_task_icon_state = {
      .is_visible = true,
      .text = tabs::ActorTaskIconState::Text::kNeedsAttention};

  task_icon_controller->OnStateUpdate(
      /*is_showing=*/false, glic::mojom::CurrentView::kConversation,
      actor_task_icon_state);

  ASSERT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsShowing());

  EXPECT_TRUE(GlicActorButtonContainer()->GetVisible());
  EXPECT_TRUE(GlicActorTaskIcon()->GetVisible());
  EXPECT_TRUE(GlicActorTaskIcon()->GetIsShowingNudge());
  EXPECT_EQ(GlicActorTaskIcon()->GetText(),
            l10n_util::GetStringUTF16(IDR_ACTOR_CHECK_TASK_NUDGE_LABEL));

  ResetAnimation(1);

  actor_task_icon_state = {.is_visible = true,
                           .text = tabs::ActorTaskIconState::Text::kDefault};

  task_icon_controller->OnStateUpdate(
      /*is_showing=*/false, glic::mojom::CurrentView::kConversation,
      actor_task_icon_state);

  EXPECT_TRUE(GlicActorButtonContainer()->GetVisible());
  EXPECT_TRUE(GlicActorTaskIcon()->GetVisible());
  // Check that GlicButton was added to the GlicActorButtonContainer.
  ASSERT_THAT(GlicActorButtonContainer()->children(), SizeIs(2));
  EXPECT_EQ(tab_strip_action_container()->GetGlicButton(),
            GlicActorButtonContainer()->children()[1]);
  EXPECT_EQ(GlicActorTaskIcon()->GetText(), std::u16string());
  EXPECT_FALSE(GlicActorTaskIcon()->GetIsShowingNudge());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerPreRedesignBrowserTest,
                       GlicActorTaskIconTooltipAndA11yText) {
  auto* task_icon_controller =
      tabs::GlicActorTaskIconController::From(browser());
  tabs::ActorTaskIconState actor_task_icon_state = {
      .is_visible = true, .text = tabs::ActorTaskIconState::Text::kDefault};

  // Show the task icon.
  task_icon_controller->OnStateUpdate(
      /*is_showing=*/false, glic::mojom::CurrentView::kConversation,
      actor_task_icon_state);

  // TODO(crbug.com/431015299): Replace with finalized strings when ready.
  EXPECT_EQ(GlicActorTaskIcon()->GetTooltipText(),
            std::u16string(u"Open Gemini in Chrome"));
  EXPECT_EQ(GlicActorTaskIcon()->GetViewAccessibility().GetCachedName(),
            std::u16string(u"Open Gemini in Chrome"));

  task_icon_controller->OnStateUpdate(/*is_showing=*/true,
                                      glic::mojom::CurrentView::kConversation,
                                      actor_task_icon_state);

  // TODO(crbug.com/431015299): Replace with finalized strings when ready.
  EXPECT_EQ(GlicActorTaskIcon()->GetTooltipText(),
            std::u16string(u"Close Gemini in Chrome"));
  EXPECT_EQ(GlicActorTaskIcon()->GetViewAccessibility().GetCachedName(),
            std::u16string(u"Close Gemini in Chrome"));
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerPreRedesignBrowserTest,
                       ActivatesTabOnGlicActorTaskIconNudgeClick) {
  auto* actor_service = actor::ActorKeyedService::Get(browser()->GetProfile());
  actor_service->GetPolicyChecker().SetActOnWebForTesting(true);
  actor::TaskId task_id = actor_service->CreateTask();
  actor::ActorTask* task = actor_service->GetTask(task_id);
  actor::ui::StartTask start_task_event(task_id);
  actor_service->GetActorUiStateManager()->OnUiEvent(start_task_event);
  // Need to wait for the AUSM to notify the GlicActorTaskIconManager.
  base::PlatformThread::Sleep(actor::ui::kProfileScopedUiUpdateDebounceDelay);

  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 0,
                                     GURL(chrome::kChromeUINewTabURL),
                                     ui::PAGE_TRANSITION_LINK));
  auto* tab_one = browser()->GetTabStripModel()->GetTabAtIndex(0);
  base::RunLoop loop;
  task->AddTab(
      tab_one->GetHandle(),
      base::BindLambdaForTesting([&](actor::mojom::ActionResultPtr result) {
        EXPECT_TRUE(actor::IsOk(*result));
        loop.Quit();
      }));
  loop.Run();

  // Add and activate the non-actuation tab.
  ASSERT_TRUE(AddTabAtIndexToBrowser(browser(), 1,
                                     GURL(chrome::kChromeUINewTabURL),
                                     ui::PAGE_TRANSITION_LINK));
  auto* tab_two = browser()->GetTabStripModel()->GetTabAtIndex(1);
  browser()->GetTabStripModel()->ActivateTabAt(1);

  EXPECT_TRUE(task->IsActingOnTab(tab_one->GetHandle()));
  EXPECT_FALSE(task->IsActingOnTab(tab_two->GetHandle()));
  EXPECT_FALSE(tab_one->IsActivated());
  EXPECT_TRUE(tab_two->IsActivated());

  actor_service->GetActorUiStateManager()->OnUiEvent(
      actor::ui::TaskStateChanged(
          task_id, actor::ActorTask::State::kPausedByActor, /*title=*/""));
  // Need to wait for the AUSM to notify the GlicActorTaskIconManager.
  base::PlatformThread::Sleep(actor::ui::kProfileScopedUiUpdateDebounceDelay);

  auto* task_icon_controller =
      tabs::GlicActorTaskIconController::From(browser());
  auto actor_task_icon_state = tabs::ActorTaskIconState();
  actor_task_icon_state = {
      .is_visible = true,
      .text = tabs::ActorTaskIconState::Text::kNeedsAttention};
  task_icon_controller->OnStateUpdate(
      /*is_showing=*/false, glic::mojom::CurrentView::kConversation,
      actor_task_icon_state);
  EXPECT_TRUE(GlicActorTaskIcon()->GetIsShowingNudge());
  OnButtonClicked(GlicActorTaskIcon());

  EXPECT_TRUE(tab_one->IsActivated());
  EXPECT_FALSE(tab_two->IsActivated());

  // Mark task as completed and remove the tab being actuated on.
  actor_task_icon_state = {
      .is_visible = true,
      .text = tabs::ActorTaskIconState::Text::kCompleteTasks};
  task_icon_controller->OnStateUpdate(
      /*is_showing=*/false, glic::mojom::CurrentView::kConversation,
      actor_task_icon_state);
  task->RemoveTab(tab_one->GetHandle());

  EXPECT_TRUE(GlicActorTaskIcon()->GetIsShowingNudge());
  EXPECT_FALSE(task->IsActingOnTab(tab_one->GetHandle()));
  // User switches to another tab but the last actuated tab has been removed
  // from the task. Expect no change in the active tab once it is removed.
  browser()->GetTabStripModel()->ActivateTabAt(1);
  OnButtonClicked(GlicActorTaskIcon());
  EXPECT_TRUE(tab_two->IsActivated());
  EXPECT_FALSE(tab_one->IsActivated());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerPreRedesignBrowserTest,
                       LogsWhenGlicActorTaskIconClicked) {
  EXPECT_FALSE(GlicActorButtonContainer()->GetVisible());
  ASSERT_THAT(GlicActorButtonContainer()->children(), SizeIs(1));

  auto* task_icon_controller =
      tabs::GlicActorTaskIconController::From(browser());
  auto actor_task_icon_state = tabs::ActorTaskIconState();
  actor_task_icon_state.is_visible = true;
  task_icon_controller->OnStateUpdate(
      /*is_showing=*/false, glic::mojom::CurrentView::kConversation,
      actor_task_icon_state);

  EXPECT_TRUE(GlicActorButtonContainer()->GetVisible());
  EXPECT_TRUE(GlicActorTaskIcon()->GetVisible());

  base::UserActionTester user_action_tester;

  OnButtonClicked(GlicActorTaskIcon());

  EXPECT_EQ(1, user_action_tester.GetActionCount("Actor.Ui.TaskIcon.Click"));
}
#endif  // BUILDFLAG(ENABLE_GLIC)
