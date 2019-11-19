// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "url/gurl.h"

namespace page_load_metrics {

PageLoadMetricsObserverContentTestHarness::
    PageLoadMetricsObserverContentTestHarness()
    : content::RenderViewHostTestHarness() {}

PageLoadMetricsObserverContentTestHarness::
    ~PageLoadMetricsObserverContentTestHarness() {}

void PageLoadMetricsObserverContentTestHarness::SetUp() {
  content::RenderViewHostTestHarness::SetUp();
  original_browser_client_ =
      content::SetBrowserClientForTesting(&browser_client_);
  SetContents(CreateTestWebContents());
  NavigateAndCommit(GURL("http://www.google.com"));
  // Page load metrics depends on UKM source URLs being recorded, so make sure
  // the SourceUrlRecorderWebContentsObserver is instantiated.
  ukm::InitializeSourceUrlRecorderForWebContents(web_contents());
  tester_ = std::make_unique<PageLoadMetricsObserverTester>(
      web_contents(), this,
      base::BindRepeating(
          &PageLoadMetricsObserverContentTestHarness::RegisterObservers,
          base::Unretained(this)));
  web_contents()->WasShown();
}

void PageLoadMetricsObserverContentTestHarness::TearDown() {
  content::SetBrowserClientForTesting(original_browser_client_);
  content::RenderViewHostTestHarness::TearDown();
}

}  // namespace page_load_metrics
