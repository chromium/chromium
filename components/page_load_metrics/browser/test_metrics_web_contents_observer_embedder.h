// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_TEST_METRICS_WEB_CONTENTS_OBSERVER_EMBEDDER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_TEST_METRICS_WEB_CONTENTS_OBSERVER_EMBEDDER_H_

#include <optional>
#include <vector>

#include "components/page_load_metrics/browser/page_load_metrics_embedder_interface.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/test/weak_mock_timer.h"
#include "url/gurl.h"

namespace page_load_metrics {

class PageLoadMetricsMemoryTracker;

class TestMetricsWebContentsObserverEmbedder
    : public PageLoadMetricsEmbedderInterface,
      public test::WeakMockTimerProvider {
 public:
  TestMetricsWebContentsObserverEmbedder();
  ~TestMetricsWebContentsObserverEmbedder() override;

  // PageLoadMetricsEmbedderInterface:
  bool IsNewTabPageUrl(const GURL& url) override;
  void RegisterObservers(PageLoadTracker* tracker,
                         content::NavigationHandle* navigation_handle) override;
  std::unique_ptr<base::OneShotTimer> CreateTimer() override;
  bool IsNoStatePrefetch(content::WebContents* web_contents) override;
  bool IsExtensionUrl(const GURL& url) override;
  bool IsSidePanel(content::WebContents* web_contents) override;
  bool IsNonTabWebUI() override;
  PageLoadMetricsMemoryTracker* GetMemoryTrackerForBrowserContext(
      content::BrowserContext* browser_context) override;

  void set_is_ntp(bool is_ntp) { is_ntp_ = is_ntp; }

  const std::vector<mojom::PageLoadTimingPtr>& updated_timings() const {
    return updated_timings_;
  }
  const std::vector<mojom::PageLoadTimingPtr>& complete_timings() const {
    return complete_timings_;
  }
  const std::vector<mojom::CpuTimingPtr>& updated_cpu_timings() const {
    return updated_cpu_timings_;
  }
  const std::vector<mojom::PageLoadTimingPtr>& updated_subframe_timings()
      const {
    return updated_subframe_timings_;
  }
  const std::vector<mojom::CustomUserTimingMarkPtr>&
  updated_custom_user_timings() const {
    return updated_custom_user_timings_;
  }

  // currently_committed_urls passed to OnStart().
  const std::vector<GURL>& observed_committed_urls_from_on_start() const {
    return observed_committed_urls_;
  }

  const std::vector<GURL>& observed_aborted_urls() const {
    return observed_aborted_urls_;
  }

  const std::vector<blink::UseCounterFeature>& observed_features() const {
    return observed_features_;
  }

  const std::optional<bool>& is_first_navigation_in_web_contents() const {
    return is_first_navigation_in_web_contents_;
  }

  const std::vector<ExtraRequestCompleteInfo>& loaded_resources() const {
    return loaded_resources_;
  }

  // committed URLs passed to FilteringPageLoadMetricsObserver::OnComplete().
  const std::vector<GURL>& completed_filtered_urls() const {
    return completed_filtered_urls_;
  }

  int count_on_enter_back_forward_cache() const {
    return count_on_enter_back_forward_cache_;
  }

 private:
  std::vector<mojom::PageLoadTimingPtr> updated_timings_;
  std::vector<mojom::PageLoadTimingPtr> updated_subframe_timings_;
  std::vector<mojom::PageLoadTimingPtr> complete_timings_;
  std::vector<mojom::CpuTimingPtr> updated_cpu_timings_;
  std::vector<mojom::CustomUserTimingMarkPtr> updated_custom_user_timings_;
  std::vector<GURL> observed_committed_urls_;
  std::vector<GURL> observed_aborted_urls_;
  std::vector<ExtraRequestCompleteInfo> loaded_resources_;
  std::vector<GURL> completed_filtered_urls_;
  std::vector<blink::UseCounterFeature> observed_features_;
  std::optional<bool> is_first_navigation_in_web_contents_;
  bool is_ntp_ = false;
  int count_on_enter_back_forward_cache_ = 0;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_TEST_METRICS_WEB_CONTENTS_OBSERVER_EMBEDDER_H_
