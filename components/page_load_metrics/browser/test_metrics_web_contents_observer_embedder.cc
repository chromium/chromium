// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/test_metrics_web_contents_observer_embedder.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"

namespace page_load_metrics {

namespace {

// Simple PageLoadMetricsObserver that copies observed PageLoadTimings into the
// provided std::vector, so they can be analyzed by unit tests.
class TimingLoggingPageLoadMetricsObserver final
    : public PageLoadMetricsObserver {
 public:
  TimingLoggingPageLoadMetricsObserver(
      std::vector<mojom::PageLoadTimingPtr>* updated_timings,
      std::vector<mojom::PageLoadTimingPtr>* updated_subframe_timings,
      std::vector<mojom::PageLoadTimingPtr>* complete_timings,
      std::vector<mojom::CpuTimingPtr>* updated_cpu_timings,
      std::vector<mojom::CustomUserTimingMarkPtr>* updated_custom_user_timings,
      std::vector<ExtraRequestCompleteInfo>* loaded_resources,
      std::vector<GURL>* observed_committed_urls,
      std::vector<GURL>* observed_aborted_urls,
      std::vector<blink::UseCounterFeature>* observed_features,
      std::optional<bool>* is_first_navigation_in_web_contents,
      int* count_on_enter_back_forward_cache)
      : updated_timings_(updated_timings),
        updated_subframe_timings_(updated_subframe_timings),
        complete_timings_(complete_timings),
        updated_cpu_timings_(updated_cpu_timings),
        updated_custom_user_timings_(updated_custom_user_timings),
        loaded_resources_(loaded_resources),
        observed_features_(observed_features),
        observed_committed_urls_(observed_committed_urls),
        observed_aborted_urls_(observed_aborted_urls),
        is_first_navigation_in_web_contents_(
            is_first_navigation_in_web_contents),
        count_on_enter_back_forward_cache_(count_on_enter_back_forward_cache) {}

  const char* GetObserverName() const override {
    static const char kName[] = "TimingLoggingPageLoadMetricsObserver";
    return kName;
  }

  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override {
    observed_committed_urls_->push_back(currently_committed_url);
    *is_first_navigation_in_web_contents_ =
        GetDelegate().IsFirstNavigationInWebContents();
    return CONTINUE_OBSERVING;
  }

  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override {
    return STOP_OBSERVING;
  }

  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override {
    return FORWARD_OBSERVING;
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

  void OnCustomUserTimingMarkObserved(
      const std::vector<mojom::CustomUserTimingMarkPtr>& timings) override {
    for (const auto& timing : timings) {
      updated_custom_user_timings_->push_back(timing->Clone());
    }
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
      const std::vector<blink::UseCounterFeature>& features) override {
    observed_features_->insert(observed_features_->end(), features.begin(),
                               features.end());
  }

  void OnDidInternalNavigationAbort(
      content::NavigationHandle* navigation_handle) override {
    observed_aborted_urls_->push_back(navigation_handle->GetURL());
  }

  ObservePolicy OnEnterBackForwardCache(
      const mojom::PageLoadTiming& timing) override {
    (*count_on_enter_back_forward_cache_)++;
    return PageLoadMetricsObserver::OnEnterBackForwardCache(timing);
  }

 private:
  const raw_ptr<std::vector<mojom::PageLoadTimingPtr>> updated_timings_;
  const raw_ptr<std::vector<mojom::PageLoadTimingPtr>>
      updated_subframe_timings_;
  const raw_ptr<std::vector<mojom::PageLoadTimingPtr>> complete_timings_;
  const raw_ptr<std::vector<mojom::CpuTimingPtr>> updated_cpu_timings_;
  const raw_ptr<std::vector<mojom::CustomUserTimingMarkPtr>>
      updated_custom_user_timings_;
  const raw_ptr<std::vector<ExtraRequestCompleteInfo>> loaded_resources_;
  const raw_ptr<std::vector<blink::UseCounterFeature>> observed_features_;
  const raw_ptr<std::vector<GURL>> observed_committed_urls_;
  const raw_ptr<std::vector<GURL>> observed_aborted_urls_;
  raw_ptr<std::optional<bool>> is_first_navigation_in_web_contents_;
  const raw_ptr<int> count_on_enter_back_forward_cache_;
};

// Test PageLoadMetricsObserver that stops observing page loads with certain
// substrings in the URL.
class FilteringPageLoadMetricsObserver final : public PageLoadMetricsObserver {
 public:
  explicit FilteringPageLoadMetricsObserver(
      std::vector<GURL>* completed_filtered_urls)
      : completed_filtered_urls_(completed_filtered_urls) {}

  const char* GetObserverName() const override {
    static const char kName[] = "FilteringPageLoadMetricsObserver";
    return kName;
  }

  ObservePolicy OnStart(content::NavigationHandle* handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override {
    const bool should_ignore =
        handle->GetURL().spec().find("ignore-on-start") != std::string::npos;
    return should_ignore ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }

  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override {
    return FORWARD_OBSERVING;
  }

  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override {
    const bool should_ignore = navigation_handle->GetURL().spec().find(
                                   "ignore-on-start") != std::string::npos;
    return should_ignore ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }

  ObservePolicy OnCommit(content::NavigationHandle* handle) override {
    const bool should_ignore =
        handle->GetURL().spec().find("ignore-on-commit") != std::string::npos;
    return should_ignore ? STOP_OBSERVING : CONTINUE_OBSERVING;
  }

  void OnComplete(const mojom::PageLoadTiming& timing) override {
    completed_filtered_urls_->push_back(GetDelegate().GetUrl());
  }

 private:
  const raw_ptr<std::vector<GURL>> completed_filtered_urls_;
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
    PageLoadTracker* tracker,
    content::NavigationHandle* navigation_handle) {
  tracker->AddObserver(std::make_unique<TimingLoggingPageLoadMetricsObserver>(
      &updated_timings_, &updated_subframe_timings_, &complete_timings_,
      &updated_cpu_timings_, &updated_custom_user_timings_, &loaded_resources_,
      &observed_committed_urls_, &observed_aborted_urls_, &observed_features_,
      &is_first_navigation_in_web_contents_,
      &count_on_enter_back_forward_cache_));
  tracker->AddObserver(std::make_unique<FilteringPageLoadMetricsObserver>(
      &completed_filtered_urls_));
}

std::unique_ptr<base::OneShotTimer>
TestMetricsWebContentsObserverEmbedder::CreateTimer() {
  auto timer = std::make_unique<test::WeakMockTimer>();
  SetMockTimer(timer->AsWeakPtr());
  return std::move(timer);
}

bool TestMetricsWebContentsObserverEmbedder::IsNoStatePrefetch(
    content::WebContents* web_contents) {
  return false;
}

bool TestMetricsWebContentsObserverEmbedder::IsExtensionUrl(const GURL& url) {
  return false;
}

bool TestMetricsWebContentsObserverEmbedder::IsSidePanel(
    content::WebContents* web_contents) {
  return false;
}

bool TestMetricsWebContentsObserverEmbedder::IsNonTabWebUI() {
  return false;
}

PageLoadMetricsMemoryTracker*
TestMetricsWebContentsObserverEmbedder::GetMemoryTrackerForBrowserContext(
    content::BrowserContext* browser_context) {
  return nullptr;
}

}  // namespace page_load_metrics
