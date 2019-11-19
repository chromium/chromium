// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/page_load_metrics/browser/test_metrics_web_contents_observer_embedder.h"

#include <memory>

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/browser/render_frame_host.h"

namespace page_load_metrics {

namespace {

// Simple PageLoadMetricsObserver that copies observed PageLoadTimings into the
// provided std::vector, so they can be analyzed by unit tests.
class TestPageLoadMetricsObserver : public PageLoadMetricsObserver {
 public:
  TestPageLoadMetricsObserver(
      std::vector<mojom::PageLoadTimingPtr>* updated_timings,
      std::vector<mojom::PageLoadTimingPtr>* updated_subframe_timings,
      std::vector<mojom::PageLoadTimingPtr>* complete_timings,
      std::vector<mojom::CpuTimingPtr>* updated_cpu_timings,
      std::vector<ExtraRequestCompleteInfo>* loaded_resources,
      std::vector<GURL>* observed_committed_urls,
      std::vector<GURL>* observed_aborted_urls,
      std::vector<mojom::PageLoadFeatures>* observed_features,
      base::Optional<bool>* is_first_navigation_in_web_contents)
      : updated_timings_(updated_timings),
        updated_subframe_timings_(updated_subframe_timings),
        complete_timings_(complete_timings),
        updated_cpu_timings_(updated_cpu_timings),
        loaded_resources_(loaded_resources),
        observed_features_(observed_features),
        observed_committed_urls_(observed_committed_urls),
        observed_aborted_urls_(observed_aborted_urls),
        is_first_navigation_in_web_contents_(
            is_first_navigation_in_web_contents) {}

  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override {
    observed_committed_urls_->push_back(currently_committed_url);
    *is_first_navigation_in_web_contents_ =
        GetDelegate().IsFirstNavigationInWebContents();
    return CONTINUE_OBSERVING;
  }

  void OnTimingUpdate(content::RenderFrameHost* subframe_rfh,
                      const mojom::PageLoadTiming& timing) override {
    if (subframe_rfh) {
      DCHECK(subframe_rfh->GetParent());
      updated_subframe_timings_->push_back(timing.Clone());
    } else {
      updated_timings_->push_back(timing.Clone());
    }
  }

  void OnCpuTimingUpdate(content::RenderFrameHost* subframe_rfh,
                         const mojom::CpuTiming& timing) override {
    updated_cpu_timings_->push_back(timing.Clone());
  }

  void OnComplete(const mojom::PageLoadTiming& timing) override {
    complete_timings_->push_back(timing.Clone());
  }

  ObservePolicy FlushMetricsOnAppEnterBackground(
      const mojom::PageLoadTiming& timing) override {
    return STOP_OBSERVING;
  }

  void OnLoadedResource(
      const ExtraRequestCompleteInfo& extra_request_complete_info) override {
    loaded_resources_->emplace_back(extra_request_complete_info);
  }

  void OnFeaturesUsageObserved(
      content::RenderFrameHost* rfh,
      const mojom::PageLoadFeatures& features) override {
    observed_features_->push_back(features);
  }

  void OnDidInternalNavigationAbort(
      content::NavigationHandle* navigation_handle) override {
    observed_aborted_urls_->push_back(navigation_handle->GetURL());
  }

 private:
  std::vector<mojom::PageLoadTimingPtr>* const updated_timings_;
  std::vector<mojom::PageLoadTimingPtr>* const updated_subframe_timings_;
  std::vector<mojom::PageLoadTimingPtr>* const complete_timings_;
  std::vector<mojom::CpuTimingPtr>* const updated_cpu_timings_;
  std::vector<ExtraRequestCompleteInfo>* const loaded_resources_;
  std::vector<mojom::PageLoadFeatures>* const observed_features_;
  std::vector<GURL>* const observed_committed_urls_;
  std::vector<GURL>* const observed_aborted_urls_;
  base::Optional<bool>* is_first_navigation_in_web_contents_;
};

// Test PageLoadMetricsObserver that stops observing page loads with certain
// substrings in the URL.
class FilteringPageLoadMetricsObserver : public PageLoadMetricsObserver {
 public:
  explicit FilteringPageLoadMetricsObserver(
      std::vector<GURL>* completed_filtered_urls)
      : completed_filtered_urls_(completed_filtered_urls) {}

  ObservePolicy OnStart(content::NavigationHandle* handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override {
    const bool should_ignore =
        handle->GetURL().spec().find("ignore-on-start") != std::string::npos;
    return should_ignore ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }

  ObservePolicy OnCommit(content::NavigationHandle* handle,
                         ukm::SourceId source_id) override {
    const bool should_ignore =
        handle->GetURL().spec().find("ignore-on-commit") != std::string::npos;
    return should_ignore ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }

  void OnComplete(const mojom::PageLoadTiming& timing) override {
    completed_filtered_urls_->push_back(GetDelegate().GetUrl());
  }

 private:
  std::vector<GURL>* const completed_filtered_urls_;
};

}  // namespace

TestMetricsWebContentsObserverEmbedder::
    TestMetricsWebContentsObserverEmbedder() = default;

TestMetricsWebContentsObserverEmbedder::
    ~TestMetricsWebContentsObserverEmbedder() = default;

bool TestMetricsWebContentsObserverEmbedder::IsNewTabPageUrl(const GURL& url) {
  return is_ntp_;
}

void TestMetricsWebContentsObserverEmbedder::RegisterObservers(
    PageLoadTracker* tracker) {
  tracker->AddObserver(std::make_unique<TestPageLoadMetricsObserver>(
      &updated_timings_, &updated_subframe_timings_, &complete_timings_,
      &updated_cpu_timings_, &loaded_resources_, &observed_committed_urls_,
      &observed_aborted_urls_, &observed_features_,
      &is_first_navigation_in_web_contents_));
  tracker->AddObserver(std::make_unique<FilteringPageLoadMetricsObserver>(
      &completed_filtered_urls_));
}

std::unique_ptr<base::OneShotTimer>
TestMetricsWebContentsObserverEmbedder::CreateTimer() {
  auto timer = std::make_unique<test::WeakMockTimer>();
  SetMockTimer(timer->AsWeakPtr());
  return std::move(timer);
}

bool TestMetricsWebContentsObserverEmbedder::IsPrerender(
    content::WebContents* web_contents) {
  return false;
}

bool TestMetricsWebContentsObserverEmbedder::IsExtensionUrl(const GURL& url) {
  return false;
}

}  // namespace page_load_metrics
