// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

// Test suite for the chrome://metrics-internals WebUI page.
// Unlike most other WebUI tests under the chrome/test/data/webui directory,
// this file tests both the frontend and the backend intentionally.
class MetricsInternalsUIBrowserTest : public WebUIMochaBrowserTest {
 protected:
  MetricsInternalsUIBrowserTest() {
    set_test_loader_host(chrome::kChromeUIMetricsInternalsHost);
  }
};

using MetricsInternalsUIBrowserTestWithoutLog = MetricsInternalsUIBrowserTest;
IN_PROC_BROWSER_TEST_F(MetricsInternalsUIBrowserTestWithoutLog, All) {
  RunTest("metrics_internals/no_logs_test.js", "mocha.run()");
}

class MetricsInternalsUIBrowserTestWithLog
    : public MetricsInternalsUIBrowserTest,
      public content::WebContentsObserver {
 protected:
  void SetUp() override {
    // Make metrics reporting work the same as in Chrome branded builds, for
    // test consistency between Chromium and Chrome builds.
    ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
        true);
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &metrics_enabled_);

    // When Chrome is run with a command line enabling a feature, metrics
    // reporting is disabled. This avoids some users to pollute UMA.
    //
    // However, MetricsInternalsUIBrowserTestWithLog.All is testing UI showing
    // reported metrics. This needs to be ignored:
    metrics::EnabledStateProvider::SetIgnoreForceFieldTrialsForTesting(true);

    // Simulate being sampled in so that metrics reporting is not disabled due
    // to being sampled out.
    feature_list_.InitAndEnableFeature(
        metrics::internal::kMetricsReportingFeature);

    MetricsInternalsUIBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    DCHECK(web_contents);
    // Note that we stop observing automatically in the destructor of
    // content::WebContentsObserver, so no need to do it manually.
    content::WebContentsObserver::Observe(web_contents);

    MetricsInternalsUIBrowserTest::SetUpOnMainThread();
  }

 private:
  // content::WebContentsObserver:
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override {
    // Close and stage a log upon finishing loading chrome://metrics-internals.
    // This will allow us to have a log ready for the JS browsertest.
    if (render_frame_host->GetLastCommittedURL().host() ==
        chrome::kChromeUIMetricsInternalsHost) {
      g_browser_process->metrics_service()->StageCurrentLogForTest();
    }
  }

  bool metrics_enabled_ = true;

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(MetricsInternalsUIBrowserTestWithLog, All) {
  RunTest("metrics_internals/with_log_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(MetricsInternalsUIBrowserTest, FieldTrials) {
  RunTest("metrics_internals/field_trials_test.js", "mocha.run()");
}
