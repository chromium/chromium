// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_GWS_PREWARM_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_GWS_PREWARM_PAGE_LOAD_METRICS_OBSERVER_H_

#include <optional>

#include "base/time/time.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

extern const char kHistogramGWSPrewarmWasResponseCached[];
extern const char kHistogramGWSPrewarmWasActivated[];
extern const char kHistogramGWSPrewarmNavigationToCancellation[];
extern const char kHistogramGWSPrewarmNavigationToDomContentLoaded[];
extern const char kHistogramGWSPrewarmNavigationToLoadEvent[];
extern const char kHistogramGWSPrewarmLoadEventOccurredBeforeCancellation[];
extern const char kHistogramGWSPrewarmLoadEventToCancellation[];
extern const char kHistogramGWSPrewarmNavigationToLastSubresourceLoad[];
extern const char kHistogramGWSPrewarmLastSubresourceLoadToCancellation[];
extern const char kHistogramGWSPrewarmSubresourceDestination[];
extern const char kHistogramGWSPrewarmSubresourceCount[];

extern const char kSubresourceSuffixCached[];
extern const char kSubresourceSuffixHttpCached[];
extern const char kSubresourceSuffixMemoryCached[];
extern const char kSubresourceSuffixNotCached[];

}  // namespace internal

// Observes prerendered Google Search Prewarm page loads and records metrics,
// including whether the page was served from HTTP cache, duration until
// cancellation, page load completion, and subresources loaded.
class GWSPrewarmPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  GWSPrewarmPageLoadMetricsObserver();
  ~GWSPrewarmPageLoadMetricsObserver() override;

  GWSPrewarmPageLoadMetricsObserver(const GWSPrewarmPageLoadMetricsObserver&) =
      delete;
  GWSPrewarmPageLoadMetricsObserver& operator=(
      const GWSPrewarmPageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver implementation:
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnRedirect(
      content::NavigationHandle* navigation_handle) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnDomContentLoadedEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadEventStart(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnLoadedResource(const page_load_metrics::ExtraRequestCompleteInfo&
                            extra_request_complete_info) override;
  void DidLoadResourceFromMemoryCache(
      const page_load_metrics::MemoryResourceLoadInfo&
          memory_resource_load_info) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo&
          failed_provisional_load_info) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;

  bool was_cached() const { return was_cached_; }
  bool is_prerendered() const { return is_prerendered_; }
  bool was_activated() const { return was_activated_; }

 private:
  enum class SubresourceCacheType {
    kNotCached,
    kHttpCached,
    kMemoryCached,
  };

  void RecordSubresourceHistograms(
      network::mojom::RequestDestination request_destination,
      SubresourceCacheType cache_type,
      std::optional<base::TimeDelta> completion_time);
  void RecordSessionEndHistograms(
      std::optional<base::TimeDelta> time_to_page_end);

  bool was_cached_ = false;
  bool is_prerendered_ = false;
  bool was_activated_ = false;

  std::optional<base::TimeDelta> load_event_start_;
  std::optional<base::TimeDelta> last_subresource_load_time_;

  int total_subresources_count_ = 0;
  int cached_subresources_count_ = 0;
  int http_cached_subresources_count_ = 0;
  int memory_cached_subresources_count_ = 0;
  int not_cached_subresources_count_ = 0;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_GOOGLE_BROWSER_GWS_PREWARM_PAGE_LOAD_METRICS_OBSERVER_H_
