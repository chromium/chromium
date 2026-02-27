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
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/ui/states/actor_task_nudge_state.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/glic_actor_nudge_controller.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager.h"
#include "chrome/browser/ui/tabs/glic_actor_task_icon_manager_factory.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/controls/rich_hover_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/glic/tab_strip_glic_actor_task_icon.h"
#include "chrome/browser/ui/views/tabs/glic/tab_strip_glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
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

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/private_ai/private_ai_service.h"
#include "chrome/browser/private_ai/private_ai_service_factory.h"
#include "components/private_ai/client.h"
#include "components/private_ai/features.h"
#include "components/private_ai/testing/mock_private_ai_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#endif  // !BUILDFLAG(IS_ANDROID)

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
            {features::kGlicRollout, {}},
            {features::kGlicFreWarming, {}},
            {features::kGlicActorUi,
             { {features::kGlicActorUiTaskIconName, "true"} }},
            {contextual_cueing::kContextualCueing, {}},
        },
        {});
  }

  void SetUp() override {
    // This will temporarily disable preloading.
    glic::GlicProfileManager::SetPrewarmingEnabledForTesting(false);
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
    glic::GlicProfileManager::SetPrewarmingEnabledForTesting(true);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
  }

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
  glic::TabStripGlicButton* GlicNudgeButton() {
    return static_cast<glic::TabStripGlicButton*>(
        tab_strip_action_container()->GetGlicButton());
  }

  glic::TabStripGlicActorTaskIcon* GlicActorTaskIcon() {
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
    if (button == GlicNudgeButton()) {
      tab_strip_action_container()->OnGlicButtonClicked();
    } else if (button == GlicActorTaskIcon()) {
      tab_strip_action_container()->OnGlicActorTaskIconClicked();
    }
  }
  void OnButtonDismissed(TabStripNudgeButton* button) {
    if (button == GlicNudgeButton()) {
      tab_strip_action_container()->OnGlicButtonDismissed();
    }
  }

  void ResetPrewarming() {
    glic::GlicProfileManager::SetPrewarmingEnabledForTesting(true);
  }

  const GURL& fre_url() { return fre_url_; }
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

  actor::ActorKeyedService* actor_service() {
    return actor::ActorKeyedService::Get(browser()->GetProfile());
  }

  actor::TaskId CreateTask() {
    actor::TaskId task_id =
        actor_service()->CreateTask(actor::NoEnterprisePolicyChecker());
    actor::ActorTask* task = actor_service()->GetTask(task_id);
    actor::ui::StartTask start_task_event(task_id);
    actor_service()->GetActorUiStateManager()->OnUiEvent(start_task_event);

    // Add tab to task.
    base::test::TestFuture<actor::mojom::ActionResultPtr> add_tab_future;
    task->AddTab(browser()->GetActiveTabInterface()->GetHandle(),
                 add_tab_future.GetCallback());
    return task_id;
  }

 protected:
  glic::GlicTestEnvironment glic_test_environment_;
  net::EmbeddedTestServer fre_server_;
  GURL fre_url_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       ImmediatelyHidesWhenGlicNudgeButtonDismissed) {
  ShowTabStripNudgeButton(GlicNudgeButton());
  GetExpansionAnimation(GlicNudgeButton())->Reset(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  SetLockedExpansionMode(LockedExpansionMode::kWillHide, GlicNudgeButton());

  OnButtonDismissed(GlicNudgeButton());
  if (base::FeatureList::IsEnabled(features::kGlicEntrypointVariations)) {
    // Animation always runs from 0 to 1, even for dismissal, so IsShowing()
    // should return true.
    EXPECT_TRUE(GetExpansionAnimation(GlicNudgeButton())->IsShowing());
  } else {
    // TODO(crbug.com/469850069): Clean up GlicEntrypointVariations.
    EXPECT_TRUE(GetExpansionAnimation(GlicNudgeButton())->IsClosing());
  }
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
  if (base::FeatureList::IsEnabled(features::kGlicTrustFirstOnboarding)) {
    GTEST_SKIP() << "Skipping for kGlicTrustFirstOnboarding";
  }
  if (base::FeatureList::IsEnabled(features::kGlicUnifiedFreScreen)) {
    // This test does not work for Unified FRE. Looking at the FRE warming code,
    // it appears that it wasn't written to work for Unified FRE.
    // FRE prewarming should be removed anyway, so there's no reason to fix
    // this; see b/426679298.
    GTEST_SKIP() << "Skipping for kGlicUnifiedFreScreen";
  }
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
  ResetPrewarming();

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

  EXPECT_EQ(1, GlicNudgeButton()->width_factor_for_testing());
  SetLockedExpansionMode(LockedExpansionMode::kWillHide, GlicNudgeButton());

  OnButtonDismissed(GlicNudgeButton());

  GetExpansionAnimation(GlicNudgeButton())->Reset(0);
  EXPECT_EQ(0, GlicNudgeButton()->width_factor_for_testing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       OnlyExpandGlicIfNotExpanded) {
  // Show the nudge and finish the expansion animation.
  ShowTabStripNudgeButton(GlicNudgeButton());
  GetExpansionAnimation(GlicNudgeButton())->Reset(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_EQ(1, GlicNudgeButton()->width_factor_for_testing());

  // Show again. Since we're already showing, the button should remain expanded.
  ShowTabStripNudgeButton(GlicNudgeButton());
  EXPECT_EQ(1, GlicNudgeButton()->width_factor_for_testing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       OnlyCollapseGlicIfNotCollapsed) {
  // Show the nudge and finish the expansion animation.
  ShowTabStripNudgeButton(GlicNudgeButton());
  GetExpansionAnimation(GlicNudgeButton())->Reset(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_EQ(1, GlicNudgeButton()->width_factor_for_testing());

  // Collapse.
  SetLockedExpansionMode(LockedExpansionMode::kWillHide, GlicNudgeButton());
  OnButtonDismissed(GlicNudgeButton());
  GetExpansionAnimation(GlicNudgeButton())->Reset(0);
  EXPECT_EQ(0, GlicNudgeButton()->width_factor_for_testing());

  // Collapse again. The button should remain collapsed.
  OnButtonDismissed(GlicNudgeButton());
  EXPECT_EQ(0, GlicNudgeButton()->width_factor_for_testing());
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
  EXPECT_TRUE(GlicNudgeButton()->GetLabelEnabledForTesting());

  // Create/activate a different widget (just calling Deactivate() on the
  // browser window isn't enough, since it will have no effect if there isn't
  // another window to become active.)
  auto widget_2 = std::make_unique<views::Widget>(
      views::Widget::InitParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                                views::Widget::InitParams::TYPE_WINDOW));
  widget_2->Activate();
  EXPECT_FALSE(GlicNudgeButton()->GetLabelEnabledForTesting());

  // Activate the browser. The button label should be enabled again.
  tab_strip_action_container()->GetWidget()->Activate();
  EXPECT_TRUE(GlicNudgeButton()->GetLabelEnabledForTesting());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       ShowAndHideGlicActorCheckTasksNudge) {
  EXPECT_EQ(GlicActorTaskIcon()->GetText(), std::u16string());
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());
  EXPECT_FALSE(GlicActorButtonContainer()->GetVisible());

  actor::TaskId task_id = CreateTask();

  actor_service()->GetTask(task_id)->Pause(/*from_actor=*/true);
  EXPECT_TRUE(
      RunUntil([&]() { return GlicActorTaskIcon()->GetIsShowingNudge(); }));
  EXPECT_EQ(GlicActorTaskIcon()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_ACTOR_TASK_NUDGE_CHECK_TASK_LABEL, 1));

  ResetAnimation(1);

  actor_service()->GetTask(task_id)->Resume();

  EXPECT_TRUE(
      RunUntil([&]() { return !GlicActorTaskIcon()->GetIsShowingNudge(); }));
  EXPECT_EQ(GlicActorTaskIcon()->GetText(), std::u16string());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       ResetGlicActorTaskNudgeOnCheckTaskToActiveStateChange) {
  base::HistogramTester histogram_tester;
  actor::TaskId task_id = CreateTask();

  actor_service()->GetTask(task_id)->Pause(/*from_actor=*/true);

  EXPECT_TRUE(
      RunUntil([&]() { return GlicActorTaskIcon()->GetIsShowingNudge(); }));
  EXPECT_EQ(GlicActorTaskIcon()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_ACTOR_TASK_NUDGE_CHECK_TASK_LABEL, 1));

  ResetAnimation(1);

  actor_service()->GetTask(task_id)->Resume();

  EXPECT_TRUE(
      RunUntil([&]() { return !GlicActorTaskIcon()->GetIsShowingNudge(); }));
  // Ensure the shown histogram was not recorded as the default state doesn't
  // show a nudge.
  EXPECT_EQ(
      histogram_tester.GetBucketCount("Actor.Ui.TaskNudge.Shown",
                                      ActorTaskNudgeState::Text::kDefault),
      0);
  // Check that Task Icon remains in the GlicActorButtonContainer.
  ASSERT_THAT(GlicActorButtonContainer()->children(), SizeIs(3));
  EXPECT_EQ(GlicActorTaskIcon(), GlicActorButtonContainer()->children()[2]);
  EXPECT_EQ(GlicActorTaskIcon()->GetText(), std::u16string());
  EXPECT_FALSE(GlicActorTaskIcon()->GetIsShowingNudge());
}

IN_PROC_BROWSER_TEST_F(
    TabStripActionContainerBrowserTest,
    GlicActorNudgeDoesNotRetriggerOnSingleTaskNeedsAttentionToMultipleTextChange) {
  EXPECT_EQ(GlicActorTaskIcon()->GetText(), std::u16string());
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());
  EXPECT_FALSE(GlicActorButtonContainer()->GetVisible());

  actor::TaskId task_id = CreateTask();

  actor_service()->GetTask(task_id)->Pause(/*from_actor=*/true);

  EXPECT_TRUE(
      RunUntil([&]() { return GlicActorTaskIcon()->GetIsShowingNudge(); }));
  EXPECT_EQ(GlicActorTaskIcon()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_ACTOR_TASK_NUDGE_CHECK_TASK_LABEL, 1));

  ResetAnimation(1);

  actor::TaskId task_id2 = CreateTask();

  actor_service()->GetTask(task_id2)->Pause(/*from_actor=*/true);

  auto* manager = tabs::GlicActorTaskIconManagerFactory::GetForProfile(
      browser()->GetProfile());
  EXPECT_TRUE(RunUntil(
      [&]() { return manager->actor_task_list_bubble_rows().size() == 2; }));
  EXPECT_TRUE(
      RunUntil([&]() { return GlicActorTaskIcon()->GetIsShowingNudge(); }));
  EXPECT_EQ(GlicActorTaskIcon()->GetText(),
            l10n_util::GetPluralStringFUTF16(
                IDS_ACTOR_TASK_NUDGE_CHECK_TASK_LABEL, 2));
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       GlicActorCompleteShowsNudge) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(GlicActorTaskIcon()->GetText(), std::u16string());
  ASSERT_FALSE(tab_strip_action_container()->animation_session_for_testing());
  EXPECT_FALSE(GlicActorButtonContainer()->GetVisible());

  auto* actor_service = actor::ActorKeyedService::Get(browser()->GetProfile());
  actor::TaskId task_id =
      actor_service->CreateTask(actor::NoEnterprisePolicyChecker());
  actor::ui::StartTask start_task_event(task_id);
  actor_service->GetActorUiStateManager()->OnUiEvent(start_task_event);
  actor_service->StopTask(task_id,
                          actor::ActorTask::StoppedReason::kTaskComplete);

  ASSERT_TRUE(RunUntil([&]() {
    return GlicActorTaskIcon()->GetText() ==
           l10n_util::GetPluralStringFUTF16(
               IDS_ACTOR_TASK_NUDGE_TASK_COMPLETE_LABEL, 1);
  }));
  EXPECT_TRUE(GlicActorButtonContainer()->GetVisible());
  EXPECT_TRUE(GlicActorTaskIcon()->GetIsShowingNudge());
  EXPECT_EQ(histogram_tester.GetBucketCount(
                "Actor.Ui.GlobalTaskIndicator.Nudge.Shown",
                ActorTaskNudgeState::Text::kCompleteTasks),
            1);
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       LogsWhenGlicActorTaskIconClicked) {
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(GlicActorButtonContainer()->GetVisible());
  ASSERT_THAT(GlicActorButtonContainer()->children(), SizeIs(2));

  actor::TaskId task_id = CreateTask();

  auto* manager = tabs::GlicActorTaskIconManagerFactory::GetForProfile(
      browser()->GetProfile());

  actor_service()->GetTask(task_id)->SetState(actor::ActorTask::State::kActing);
  actor_service()->GetTask(task_id)->Interrupt();
  manager->UpdateTaskIconComponents(task_id);

  EXPECT_TRUE(
      RunUntil([&]() { return GlicActorTaskIcon()->GetIsShowingNudge(); }));
  EXPECT_TRUE(GlicActorButtonContainer()->GetVisible());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Actor.Ui.GlobalTaskIndicator.Nudge.Shown",
               ActorTaskNudgeState::Text::kNeedsAttention) == 1;
  }));

  base::UserActionTester user_action_tester;
  OnButtonClicked(GlicActorTaskIcon());
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Actor.Ui.GlobalTaskIndicator.NeedsAttention.Click"));

  // Ensure no traffic is going to the TaskNudge metrics.
  EXPECT_EQ(histogram_tester.GetBucketCount(
                "Actor.Ui.TaskNudge.Shown",
                ActorTaskNudgeState::Text::kNeedsAttention),
            0);
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "Actor.Ui.TaskNudge.NeedsAttention.Click"));
}

#if !BUILDFLAG(IS_ANDROID)
class TabStripActionContainerPrivateAiBrowserTest
    : public TabStripActionContainerBrowserTest {
 public:
  TabStripActionContainerPrivateAiBrowserTest() {
    private_ai_feature_list_.InitWithFeaturesAndParameters(
        {{private_ai::kPrivateAi,
          {{private_ai::kPrivateAiApiKey.name, "test-api-key"}}},
         {contextual_cueing::kZeroStateSuggestionsUsePrivateAi, {}}},
        {});
  }

 private:
  base::test::ScopedFeatureList private_ai_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabStripActionContainerPrivateAiBrowserTest,
                       EstablishesPrivateAiConnectionOnGlicButtonHover) {
  auto* private_ai_service = private_ai::PrivateAiServiceFactory::GetForProfile(
      browser()->GetProfile());
  ASSERT_TRUE(private_ai_service);
  auto mock_client =
      std::make_unique<testing::StrictMock<private_ai::MockPrivateAiClient>>();
  auto* mock_client_ptr = mock_client.get();
  private_ai_service->SetClientForTesting(std::move(mock_client));

  EXPECT_CALL(*mock_client_ptr, EstablishConnection());

  // Hover over the glic button.
  ui::MouseEvent mouse_enter(ui::EventType::kMouseEntered, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  GlicNudgeButton()->OnMouseEntered(mouse_enter);
}

#endif  // !BUILDFLAG(IS_ANDROID)
