// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "components/user_education/views/help_bubble_factory_views.h"
#include "components/user_education/views/help_bubble_view.h"
#include "content/public/test/browser_test.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_utils.h"
#include "url/gurl.h"

class HighEfficiencyHelpPromoTest : public InProcessBrowserTest {
 public:
  HighEfficiencyHelpPromoTest() = default;
  ~HighEfficiencyHelpPromoTest() override = default;
  // using performance_manager::features;

  void SetUp() override {
    feature_list_.InitWithFeatures(
        {feature_engagement::kIPHHighEfficiencyModeFeature,
         performance_manager::features::kHighEfficiencyModeAvailable},
        {});

    InProcessBrowserTest::SetUp();
  }

  void TearDown() override { InProcessBrowserTest::TearDown(); }

  BrowserFeaturePromoController* GetFeaturePromoController() {
    auto* promo_controller = static_cast<BrowserFeaturePromoController*>(
        browser()->window()->GetFeaturePromoController());
    return promo_controller;
  }

  void PressButton(views::Button* button) {
    views::test::InteractionTestUtilSimulatorViews::PressButton(
        button, ui::test::InteractionTestUtil::InputType::kMouse);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Check that the high efficiency mode in-product help promo is shown when
// a tab threshold (10) is reached and dismisses correctly when the app menu
// button is pushed
IN_PROC_BROWSER_TEST_F(HighEfficiencyHelpPromoTest, ShowPromoOnTabThreshold) {
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  int tab_count_threshold =
      performance_manager::features::kHighEfficiencyModePromoTabCountThreshold
          .Get();
  for (int i = 0; i < tab_count_threshold; i++)
    chrome::AddTabAt(browser(), GURL(), i, true);

  base::RunLoop().RunUntilIdle();

  auto* const promo_controller = GetFeaturePromoController();
  bool promo_active = user_education::test::WaitForStartupPromo(
      promo_controller, feature_engagement::kIPHHighEfficiencyModeFeature);
  EXPECT_TRUE(promo_active);

  auto* app_menu_button_view =
      views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
          kAppMenuButtonElementId, browser()->window()->GetElementContext());
  PressButton(views::AsViewClass<views::Button>(app_menu_button_view));

  base::RunLoop().RunUntilIdle();

  promo_active = promo_controller->IsPromoActive(
      feature_engagement::kIPHHighEfficiencyModeFeature);
  EXPECT_FALSE(promo_active);
}

// Confirm that High Efficiency mode is enabled when the custom action
// button for high efficiency mode is clicked
IN_PROC_BROWSER_TEST_F(HighEfficiencyHelpPromoTest, PromoCustomActionClicked) {
  PrefService* prefs = g_browser_process->local_state();
  EXPECT_TRUE(prefs
                  ->FindPreference(performance_manager::user_tuning::prefs::
                                       kHighEfficiencyModeEnabled)
                  ->IsDefaultValue());
  EXPECT_FALSE(prefs->GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled));

  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  int tab_count_threshold =
      performance_manager::features::kHighEfficiencyModePromoTabCountThreshold
          .Get();
  for (int i = 0; i < tab_count_threshold; i++)
    chrome::AddTabAt(browser(), GURL(), i, true);

  base::RunLoop().RunUntilIdle();

  auto* const promo_controller = GetFeaturePromoController();
  bool promo_active = user_education::test::WaitForStartupPromo(
      promo_controller, feature_engagement::kIPHHighEfficiencyModeFeature);
  EXPECT_TRUE(promo_active);

  auto* promo_bubble = promo_controller->promo_bubble_for_testing()
                           ->AsA<user_education::HelpBubbleViews>()
                           ->bubble_view();
  auto* custom_action_button = promo_bubble->GetDefaultButtonForTesting();
  PressButton(custom_action_button);

  EXPECT_FALSE(prefs
                   ->FindPreference(performance_manager::user_tuning::prefs::
                                        kHighEfficiencyModeEnabled)
                   ->IsDefaultValue());
  EXPECT_TRUE(prefs->GetBoolean(
      performance_manager::user_tuning::prefs::kHighEfficiencyModeEnabled));
}
