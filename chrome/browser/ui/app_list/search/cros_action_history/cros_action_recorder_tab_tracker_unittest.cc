// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/command_line.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/cros_action_history/cros_action_recorder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "base/strings/strcat.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_activity_simulator.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/test_browser_window.h"

namespace app_list {

const std::vector<GURL>& TestUrls() {
  static base::NoDestructor<std::vector<GURL>> test_urls{
      {GURL("https://test0.example.com"), GURL("https://test1.example.com")}};
  return *test_urls;
}

// Test functions of CrOSActionRecorder.
class CrOSActionRecorderTabTrackerTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    // Set the flag.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ash::switches::kEnableCrOSActionRecorder,
        ash::switches::kCrOSActionRecorderWithoutHash);

    // Initialize CrOSActionRecorder.
    CrOSActionRecorder::GetCrosActionRecorder()->Init(profile());

    // Initialize browser.
    browser_ = CreateBrowserWithTestWindowForParams(
        Browser::CreateParams(profile(), true));
    tab_strip_model_ = browser_->tab_strip_model();
  }

  void TearDown() override {
    tab_strip_model_->CloseAllTabs();
    browser_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  CrOSActionHistoryProto GetCrOSActionHistory() {
    return CrOSActionRecorder::GetCrosActionRecorder()->actions_;
  }

  // Add a tab with url as TestUrls[i] and navigate to it immediately.
  void AddNewTab(const int i) {
    tab_activity_simulator_.AddWebContentsAndNavigate(tab_strip_model_,
                                                      TestUrls()[i]);
    if (i == 0)
      tab_strip_model_->ActivateTabAt(i);
    else
      tab_activity_simulator_.SwitchToTabAt(tab_strip_model_, i);
  }

  void ExpectCrOSAction(const CrOSActionProto& action,
                        const std::string& action_name,
                        const int url_index) {
    // Expected action_name should be action_name-url;
    const std::string expected_action_name =
        base::StrCat({action_name, "-", TestUrls()[url_index].spec()});
    EXPECT_EQ(action.action_name(), expected_action_name);
  }

  TabActivitySimulator tab_activity_simulator_;
  TabStripModel* tab_strip_model_;
  std::unique_ptr<Browser> browser_;
};

// Check navigations and reactivations are logged properly.
TEST_F(CrOSActionRecorderTabTrackerTest, NavigateAndForegroundTest) {
  // Add and activate the first tab will only log a TabNavigated event.
  AddNewTab(0);
  ExpectCrOSAction(GetCrOSActionHistory().actions(0), "TabNavigated", 0);

  // Add and activate the second tab will log a TabNavigated event, and then a
  // TabReactivated event.
  AddNewTab(1);
  ExpectCrOSAction(GetCrOSActionHistory().actions(1), "TabNavigated", 1);
  ExpectCrOSAction(GetCrOSActionHistory().actions(2), "TabReactivated", 1);

  // Switch to tab@0 will log a TabReactivated event.
  tab_activity_simulator_.SwitchToTabAt(tab_strip_model_, 0);
  ExpectCrOSAction(GetCrOSActionHistory().actions(3), "TabReactivated", 0);

  // Check no extra events are logged.
  EXPECT_EQ(GetCrOSActionHistory().actions_size(), 4);
}

}  // namespace app_list
