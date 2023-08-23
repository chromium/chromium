// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/performance_manager/public/features.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/controls/menu/menu_item_view.h"
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
    feature_list_.InitAndEnableFeatures(
        {feature_engagement::kIPHPerformanceNewBadgeFeature});
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

 private:
  feature_engagement::test::ScopedIphFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(AppMenuInteractiveTest, PerformanceShowsNewBadge) {
  ASSERT_TRUE(user_education::test::WaitForFeatureEngagementReady(
      GetFeaturePromoController()));

  RunTestSequence(
      InstrumentTab(kPrimaryTabPageElementId),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kMoreToolsMenuItem),
      AfterShow(ToolsMenuModel::kPerformanceMenuItem,
                base::BindOnce([](ui::TrackedElement* el) {
                  EXPECT_TRUE(AsView<views::MenuItemView>(el)->is_new());
                })),
      SelectMenuItem(ToolsMenuModel::kPerformanceMenuItem),
      WaitForWebContentsNavigation(
          kPrimaryTabPageElementId,
          GURL(chrome::kChromeUIPerformanceSettingsURL)),
      PressButton(kToolbarAppMenuButtonElementId),
      SelectMenuItem(AppMenuModel::kMoreToolsMenuItem),
      AfterShow(ToolsMenuModel::kPerformanceMenuItem,
                base::BindOnce([](ui::TrackedElement* el) {
                  EXPECT_TRUE(AsView<views::MenuItemView>(el)->is_new());
                })));
}
