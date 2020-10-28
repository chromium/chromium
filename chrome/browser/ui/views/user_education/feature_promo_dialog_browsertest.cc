// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/global_media_controls/media_toolbar_button_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "chrome/common/buildflags.h"
#include "components/feature_engagement/public/feature_list.h"
#include "components/feature_engagement/test/mock_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
#include "ui/base/pointer/touch_ui_controller.h"
#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::NiceMock;
using ::testing::Ref;
using ::testing::Return;

class FeaturePromoDialogTest : public DialogBrowserTest {
 public:
  FeaturePromoDialogTest() {
    // TODO(crbug.com/1141984): fix cause of bubbles overflowing the
    // screen and remove this.
    set_should_verify_dialog_bounds(false);
  }

  ~FeaturePromoDialogTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    auto* mock_tracker = static_cast<feature_engagement::test::MockTracker*>(
        feature_engagement::TrackerFactory::GetForBrowserContext(
            browser()->profile()));
    ASSERT_TRUE(mock_tracker);

    FeaturePromoControllerViews* promo_controller =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->feature_promo_controller();
    ASSERT_TRUE(promo_controller);

    // Look up the IPH name and get the base::Feature.
    std::vector<const base::Feature*> iph_features =
        feature_engagement::GetAllFeatures();
    auto feature_it =
        std::find_if(iph_features.begin(), iph_features.end(),
                     [&](const base::Feature* f) { return f->name == name; });
    ASSERT_NE(feature_it, iph_features.end());
    const base::Feature& feature = **feature_it;

    // Set up mock tracker to allow the IPH, then attempt to show it.
    EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(Ref(feature)))
        .Times(1)
        .WillOnce(Return(true));
    ASSERT_TRUE(promo_controller->MaybeShowPromo(feature));
  }

 private:
  static void RegisterMockTracker(content::BrowserContext* context) {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(CreateMockTracker));
  }

  static std::unique_ptr<KeyedService> CreateMockTracker(
      content::BrowserContext* context) {
    auto mock_tracker =
        std::make_unique<NiceMock<feature_engagement::test::MockTracker>>();

    // Allow calls for other IPH.
    EXPECT_CALL(*mock_tracker, ShouldTriggerHelpUI(_))
        .Times(AnyNumber())
        .WillRepeatedly(Return(false));

    return mock_tracker;
  }

  std::unique_ptr<
      BrowserContextDependencyManager::CreateServicesCallbackList::Subscription>
      subscription_{BrowserContextDependencyManager::GetInstance()
                        ->RegisterCreateServicesCallbackForTesting(
                            base::BindRepeating(RegisterMockTracker))};
};

// Adding new tests for your promo
//
// When you add a new IPH, add a test for your promo. In most cases you
// can follow these steps:
//
// 1. Get the feature name for your IPH. This will be the of the form
//    IPH_<name>. It should be the same as defined in
//    //components/feature_engagement/public/feature_constants.cc.
//
// 2. Define a new test below with the name InvokeUi_IPH_<name>. Place
//    it in alphabetical order.
//
// 3. Call set_baseline("<cl-number>"). This enables Skia Gold pixel
//    tests for your IPH.
//
// 4. Call ShowAndVerifyUi().
//
// For running your test reference the docs in
// //chrome/browser/ui/test/test_browser_dialog.h

IN_PROC_BROWSER_TEST_F(FeaturePromoDialogTest,
                       InvokeUi_IPH_DesktopTabGroupsNewGroup) {
  set_baseline("2473537");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FeaturePromoDialogTest, InvokeUi_IPH_LiveCaption) {
  if (!base::FeatureList::IsEnabled(media::kLiveCaption))
    return;

  BrowserView::GetBrowserViewForBrowser(browser())
      ->toolbar()
      ->media_button()
      ->Show();
  RunScheduledLayouts();

  set_baseline("2473537");
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(FeaturePromoDialogTest, InvokeUi_IPH_ReopenTab) {
  set_baseline("2473537");
  ShowAndVerifyUi();
}

#if BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)

// Need a separate fixture to override the feature flag.
class FeaturePromoDialogWebUITabStripTest : public FeaturePromoDialogTest {
 public:
  FeaturePromoDialogWebUITabStripTest() {
    feature_list_.InitAndEnableFeature(features::kWebUITabStrip);
  }

  ~FeaturePromoDialogWebUITabStripTest() override = default;

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(FeaturePromoDialogWebUITabStripTest,
                       InvokeUi_IPH_WebUITabStrip) {
  ui::TouchUiController::TouchUiScoperForTesting touch_override(true);
  RunScheduledLayouts();

  set_baseline("2473537");
  ShowAndVerifyUi();
}

#endif  // BUILDFLAG(ENABLE_WEBUI_TAB_STRIP)
