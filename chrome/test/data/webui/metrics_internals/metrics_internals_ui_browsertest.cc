// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/data/webui/metrics_internals/metrics_internals_ui_browsertest.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/metrics/metrics_service.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/gurl.h"

void MetricsInternalsUIBrowserTest::SetUp() {
  // Make metrics reporting work the same as in Chrome branded builds, for test
  // consistency between Chromium and Chrome builds.
  ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
      true);
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &metrics_enabled_);

  // Simulate being sampled in so that metrics reporting is not disabled due to
  // being sampled out.
  feature_list_.InitAndEnableFeature(
      metrics::internal::kMetricsReportingFeature);

  WebUIBrowserTest::SetUp();
}

void MetricsInternalsUIBrowserTest::SetUpOnMainThread() {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  DCHECK(web_contents);
  // Note that we stop observing automatically in the destructor of
  // content::WebContentsObserver, so no need to do it manually.
  content::WebContentsObserver::Observe(web_contents);

  WebUIBrowserTest::SetUpOnMainThread();
}

void MetricsInternalsUIBrowserTest::DOMContentLoaded(
    content::RenderFrameHost* render_frame_host) {
  // Close and stage a log upon finishing loading chrome://metrics-internals.
  // This will allow us to have a log ready for the JS browsertest.
  if (render_frame_host->GetLastCommittedURL().host() ==
      chrome::kChromeUIMetricsInternalsHost) {
    g_browser_process->metrics_service()->StageCurrentLogForTest();
  }
}
