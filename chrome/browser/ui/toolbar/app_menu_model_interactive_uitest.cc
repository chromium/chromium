// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/accelerator_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/interaction/tracked_element_webcontents.h"
#include "chrome/test/interaction/webcontents_interaction_test_util.h"
#include "components/performance_manager/public/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence.h"
#include "url/gurl.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kPrimaryTabPageElementId);
}  // namespace

class AppMenuModelInteractiveTest : public InteractiveBrowserTest {
 public:
  AppMenuModelInteractiveTest() = default;
  ~AppMenuModelInteractiveTest() override = default;
  AppMenuModelInteractiveTest(const AppMenuModelInteractiveTest&) = delete;
  void operator=(const AppMenuModelInteractiveTest&) = delete;

  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        performance_manager::features::kHighEfficiencyModeAvailable);
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

 protected:
  auto CheckInconitoWindowOpened() {
    return Check(base::BindLambdaForTesting([]() {
      Browser* new_browser;
      if (BrowserList::GetIncognitoBrowserCount() == 1) {
        new_browser = BrowserList::GetInstance()->GetLastActive();
      } else {
        new_browser = ui_test_utils::WaitForBrowserToOpen();
      }
      return new_browser->profile()->IsIncognitoProfile();
    }));
  }
};

IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest, PerformanceNavigation) {
  RunTestSequence(InstrumentTab(kPrimaryTabPageElementId),
                  PressButton(kAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kMoreToolsMenuItem),
                  SelectMenuItem(ToolsMenuModel::kPerformanceMenuItem),
                  WaitForWebContentsNavigation(
                      kPrimaryTabPageElementId,
                      GURL(chrome::kChromeUIPerformanceSettingsURL)));
}

IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest, IncognitoMenuItem) {
  RunTestSequence(PressButton(kAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kIncognitoMenuItem),
                  CheckInconitoWindowOpened());
}

IN_PROC_BROWSER_TEST_F(AppMenuModelInteractiveTest, IncognitoAccelerator) {
  ui::Accelerator incognito_accelerator;
  chrome::AcceleratorProviderForBrowser(browser())->GetAcceleratorForCommandId(
      IDC_NEW_INCOGNITO_WINDOW, &incognito_accelerator);

  RunTestSequence(
      SendAccelerator(kAppMenuButtonElementId, incognito_accelerator),
      CheckInconitoWindowOpened());
}
