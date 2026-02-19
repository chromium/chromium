// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updates/update_metrics_provider.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/upgrade_detector/upgrade_detector.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using PendingUpdateState = UpdateMetricsProvider::PendingUpdateState;
using PageBlockingUpdate = UpdateMetricsProvider::PageBlockingUpdate;

class TestUpgradeDetector : public UpgradeDetector {
 public:
  void SetUpgradeDetectedTime(base::Time time) {
    set_upgrade_detected_time(time);
  }

  void SetUpgradeAvailableNone() {
    set_upgrade_available(UPGRADE_AVAILABLE_NONE);
  }

  void SetUpgradeAvailableRegular() {
    set_upgrade_available(UPGRADE_AVAILABLE_REGULAR);
  }
};

}  // namespace

class UpdateMetricsProviderBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    detector_ =
        static_cast<TestUpgradeDetector*>(UpgradeDetector::GetInstance());
    // Reset state.
    detector_->SetUpgradeAvailableNone();
  }

  void TearDownOnMainThread() override {
    detector_->SetUpgradeAvailableNone();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void SetUpgradeAvailable() {
    detector_->SetUpgradeAvailableRegular();
    detector_->SetUpgradeDetectedTime(base::Time::Now() - base::Hours(2));
  }

  void AddTab(const GURL& url) { AddTabInBrowser(browser(), url); }

  void AddTabInBrowser(Browser* browser, const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void NavigateToURL(const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  raw_ptr<TestUpgradeDetector> detector_;
  base::HistogramTester histogram_tester_;
  UpdateMetricsProvider provider_;
};

IN_PROC_BROWSER_TEST_F(UpdateMetricsProviderBrowserTest, NoUpdate) {
  provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample("Chrome.BuildState.PendingUpdateState",
                                       PendingUpdateState::kNoUpdate, 1);
  histogram_tester_.ExpectTotalCount(
      "Chrome.BuildState.TimeSinceUpdateAvailable", 0);
}

IN_PROC_BROWSER_TEST_F(UpdateMetricsProviderBrowserTest,
                       UpdateAvailableOneWindowOneTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetUpgradeAvailable();
  NavigateToURL(embedded_test_server()->GetURL("/title1.html"));

  provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample("Chrome.BuildState.PendingUpdateState",
                                       PendingUpdateState::kOneWindowOneTab, 1);
  histogram_tester_.ExpectUniqueSample("Chrome.BuildState.PageBlockingUpdate",
                                       PageBlockingUpdate::kUnspecified, 1);
  histogram_tester_.ExpectTotalCount(
      "Chrome.BuildState.TimeSinceUpdateAvailable", 1);
}

IN_PROC_BROWSER_TEST_F(UpdateMetricsProviderBrowserTest,
                       UpdateAvailableMultipleTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetUpgradeAvailable();
  AddTab(embedded_test_server()->GetURL("/title1.html"));

  provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample(
      "Chrome.BuildState.PendingUpdateState",
      PendingUpdateState::kMultipleTabsOrWindows, 1);
}

IN_PROC_BROWSER_TEST_F(UpdateMetricsProviderBrowserTest,
                       UpdateAvailableMultipleWindows) {
  SetUpgradeAvailable();

  // Create a second browser window.
  Browser* browser2 = CreateBrowser(browser()->profile());
  AddTabInBrowser(browser2, GURL("about:blank"));

  provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample(
      "Chrome.BuildState.PendingUpdateState",
      PendingUpdateState::kMultipleTabsOrWindows, 1);

  // Browser 2 is automatically cleaned up by InProcessBrowserTest, or we can
  // close it. InProcessBrowserTest closes all browsers in TearDown.
}

IN_PROC_BROWSER_TEST_F(UpdateMetricsProviderBrowserTest,
                       UpdateAvailableNtpOnly) {
  SetUpgradeAvailable();
  NavigateToURL(GURL(chrome::kChromeUINewTabURL));

  provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample("Chrome.BuildState.PendingUpdateState",
                                       PendingUpdateState::kOneWindowOneTab, 1);
  histogram_tester_.ExpectUniqueSample("Chrome.BuildState.PageBlockingUpdate",
                                       PageBlockingUpdate::kNtp, 1);
}

IN_PROC_BROWSER_TEST_F(UpdateMetricsProviderBrowserTest,
                       UpdateAvailableAboutBlank) {
  // Initial tab is about:blank.
  SetUpgradeAvailable();

  provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample("Chrome.BuildState.PendingUpdateState",
                                       PendingUpdateState::kOneWindowOneTab, 1);
  histogram_tester_.ExpectUniqueSample("Chrome.BuildState.PageBlockingUpdate",
                                       PageBlockingUpdate::kAboutBlank, 1);
}

IN_PROC_BROWSER_TEST_F(UpdateMetricsProviderBrowserTest,
                       OnlyReportsBackgroundWithNoWindow) {
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::NOTIFICATION, KeepAliveRestartOption::DISABLED);
  // Initial tab is about:blank.
  SetUpgradeAvailable();

  provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample("Chrome.BuildState.PendingUpdateState",
                                       PendingUpdateState::kOneWindowOneTab, 1);
  histogram_tester_.ExpectUniqueSample("Chrome.BuildState.PageBlockingUpdate",
                                       PageBlockingUpdate::kAboutBlank, 1);
}

IN_PROC_BROWSER_TEST_F(UpdateMetricsProviderBrowserTest,
                       UpdateAvailableChromeScheme) {
  SetUpgradeAvailable();
  NavigateToURL(GURL("chrome://chrome-urls"));

  provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample("Chrome.BuildState.PendingUpdateState",
                                       PendingUpdateState::kOneWindowOneTab, 1);
  histogram_tester_.ExpectUniqueSample("Chrome.BuildState.PageBlockingUpdate",
                                       PageBlockingUpdate::kChromeScheme, 1);
}

IN_PROC_BROWSER_TEST_F(UpdateMetricsProviderBrowserTest,
                       UpdateAvailableWhatsNew) {
  SetUpgradeAvailable();
  NavigateToURL(GURL(chrome::kChromeUIWhatsNewURL));

  provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample("Chrome.BuildState.PendingUpdateState",
                                       PendingUpdateState::kOneWindowOneTab, 1);
  histogram_tester_.ExpectUniqueSample("Chrome.BuildState.PageBlockingUpdate",
                                       PageBlockingUpdate::kWhatsNew, 1);
}

IN_PROC_BROWSER_TEST_F(UpdateMetricsProviderBrowserTest, RunInBackground) {
  // Need to use one of the types that outputs the enum.
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::NOTIFICATION, KeepAliveRestartOption::DISABLED);
  Profile* const profile = browser()->GetProfile();
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);
  CloseBrowserSynchronously(browser());

  SetUpgradeAvailable();
  provider_.ProvideCurrentSessionData(nullptr);
  histogram_tester_.ExpectUniqueSample("Chrome.BuildState.PendingUpdateState",
                                       PendingUpdateState::kBackgrounded, 1);

  chrome::NewEmptyWindow(profile);
}
