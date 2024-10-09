// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace page_load_metrics {

PageLoadMetricsObserverContentTestHarness::
    PageLoadMetricsObserverContentTestHarness()
    : RenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {
          {blink::features::kFencedFrames, {{"implementation_type", "mparch"}}},
      },
      {
          // Disable the memory requirement of Prerender2
          // so the test can run on any bot.
          {blink::features::kPrerender2MemoryControls},
      });
}

PageLoadMetricsObserverContentTestHarness::
    ~PageLoadMetricsObserverContentTestHarness() = default;

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
