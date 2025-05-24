// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_action_container.h"

#include "base/feature_list.h"
#include "base/memory/memory_pressure_monitor.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/glic_nudge_controller.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_service.h"
#include "chrome/browser/ui/tabs/organization/tab_organization_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/glic_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/optimization_guide/core/model_execution/model_execution_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/animation/slide_animation.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/fre/glic_fre.mojom.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/glic_keyed_service_factory.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_profile_manager.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#endif  // BUILDFLAG(ENABLE_GLIC)

class TabStripActionContainerBrowserTest : public InProcessBrowserTest {
 public:
  TabStripActionContainerBrowserTest() {
    feature_list_.InitWithFeatures(
        {
            features::kTabOrganization,
#if BUILDFLAG(ENABLE_GLIC)
            features::kGlic,
            features::kGlicRollout,
            features::kGlicFreWarming,
#endif
            features::kTabstripComboButton,
            features::kTabstripDeclutter,
            contextual_cueing::kContextualCueing,
        },
        {});
    TabOrganizationUtils::GetInstance()->SetIgnoreOptGuideForTesting(true);
  }

#if BUILDFLAG(ENABLE_GLIC)
  void SetUp() override {
    // This will temporarily disable preloading.
    glic::GlicProfileManager::ForceMemoryPressureForTesting(
        base::MemoryPressureMonitor::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_CRITICAL);
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

    glic_test_environment_ =
        std::make_unique<glic::GlicTestEnvironment>(browser()->profile());
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    glic_test_environment_.reset();
  }
#endif

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&TabStripActionContainerBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabStripActionContainer* tab_strip_action_container() {
    return browser_view()
        ->tab_strip_region_view()
        ->GetTabStripActionContainer();
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

  void ShowTabStripNudgeButton(TabStripNudgeButton* button) {
    tab_strip_action_container()->ShowTabStripNudge(button);
  }
  void HideTabStripNudgeButton(TabStripNudgeButton* button) {
    tab_strip_action_container()->HideTabStripNudge(button);
  }

  void OnTabStripNudgeButtonTimeout(TabStripNudgeButton* button) {
    tab_strip_action_container()->OnTabStripNudgeButtonTimeout(button);
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
        base::MemoryPressureMonitor::MemoryPressureLevel::
            MEMORY_PRESSURE_LEVEL_NONE);
  }

  const GURL& fre_url() { return fre_url_; }
#endif
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

 private:
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  base::CallbackListSubscription create_services_subscription_;
#if BUILDFLAG(ENABLE_GLIC)
  std::unique_ptr<glic::GlicTestEnvironment> glic_test_environment_;
  net::EmbeddedTestServer fre_server_;
  GURL fre_url_;
#endif
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
  ResetAnimation(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  SetLockedExpansionMode(LockedExpansionMode::kWillHide, GlicNudgeButton());

  OnButtonDismissed(GlicNudgeButton());

  EXPECT_TRUE(tab_strip_action_container()
                  ->animation_session_for_testing()
                  ->expansion_animation()
                  ->IsClosing());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       LogsWhenGlicNudgeButtonClicked) {
  ShowTabStripNudgeButton(GlicNudgeButton());

  ResetAnimation(1);
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
  auto& window_controller = service->window_controller();
  glic::SetFRECompletion(browser()->profile(),
                         glic::prefs::FreStatus::kNotStarted);
  EXPECT_TRUE(window_controller.fre_controller()->ShouldShowFreDialog());
  EXPECT_FALSE(window_controller.fre_controller()->IsWarmed());

  // This will enable preloading again.
  ResetMemoryPressure();

  base::RunLoop run_loop;
  auto subscription =
      window_controller.fre_controller()->AddWebUiStateChangedCallback(
          base::BindRepeating(
              [](base::RunLoop* run_loop,
                 glic::mojom::FreWebUiState new_state) {
                if (new_state == glic::mojom::FreWebUiState::kReady) {
                  run_loop->Quit();
                }
              },
              base::Unretained(&run_loop)));

  tab_strip_action_container()->OnTriggerGlicNudgeUI("test");
  ShowTabStripNudgeButton(GlicNudgeButton());

  // Wait for the FRE to preload.
  run_loop.Run();
  EXPECT_TRUE(window_controller.fre_controller()->IsWarmed());
}

IN_PROC_BROWSER_TEST_F(TabStripActionContainerBrowserTest,
                       ShowAndHideGlicButtonWhenGlicNudgeButtonShows) {
  ShowTabStripNudgeButton(GlicNudgeButton());

  ResetAnimation(1);
  tab_strip_action_container()->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(1, tab_strip_action_container()
                   ->GetGlicButton()
                   ->width_factor_for_testing());
  SetLockedExpansionMode(LockedExpansionMode::kWillHide, GlicNudgeButton());

  OnButtonDismissed(GlicNudgeButton());

  ResetAnimation(0);
  EXPECT_EQ(0, tab_strip_action_container()
                   ->GetGlicButton()
                   ->width_factor_for_testing());
}
#endif  // BUILDFLAG(ENABLE_GLIC)
