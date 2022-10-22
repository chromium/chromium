// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/browser_app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interaction_test_util_browser.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/performance_manager/public/features.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabPageElementId);
}  // namespace

class AppMenuInteractiveTest : public InteractiveBrowserTest {
 public:
  AppMenuInteractiveTest() = default;
  ~AppMenuInteractiveTest() override = default;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatures(
        {performance_manager::features::kHighEfficiencyModeAvailable,
         feature_engagement::kIPHPerformanceNewBadgeFeature},
        {});
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InteractiveBrowserTest::TearDownOnMainThread();
  }

  BrowserFeaturePromoController* GetFeaturePromoController() {
    auto* const promo_controller = static_cast<BrowserFeaturePromoController*>(
        browser()->window()->GetFeaturePromoController());
    return promo_controller;
  }
};

IN_PROC_BROWSER_TEST_F(AppMenuInteractiveTest, PerformanceShowsNewBadge) {
  ASSERT_TRUE(user_education::test::WaitForFeatureEngagementReady(
      GetFeaturePromoController()));

  InstrumentTab(browser(), kPrimaryTabPageElementId);
  RunTestSequence(
      WaitForWebContentsReady(kPrimaryTabPageElementId),
      PressButton(kAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kMoreToolsMenuItem),
      AfterShow(ToolsMenuModel::kPerformanceMenuItem,
                base::BindOnce([](ui::TrackedElement* el) {
                  EXPECT_TRUE(AsView<views::MenuItemView>(el)->is_new());
                })),
      SelectMenuItem(ToolsMenuModel::kPerformanceMenuItem),
      WaitForWebContentsNavigation(
          kPrimaryTabPageElementId,
          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      PressButton(kAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kMoreToolsMenuItem),
      AfterShow(ToolsMenuModel::kPerformanceMenuItem,
                base::BindOnce([](ui::TrackedElement* el) {
                  EXPECT_TRUE(AsView<views::MenuItemView>(el)->is_new());
                })));
}
