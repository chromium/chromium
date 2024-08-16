// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_UNSTARTED_PAGE_PAINT_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_UNSTARTED_PAGE_PAINT_OBSERVER_H_

#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"

namespace internal {

extern const char kPageLoadUnstartedPagePaint[];
inline constexpr uint8_t kUnstartedPagePaintTimeoutSeconds = 30u;

}  // namespace internal

// Observer responsible tracking the completion of a navigation to paint defined
// as a navigation reaching a first contentful paint event.
class UnstartedPagePaintObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  UnstartedPagePaintObserver();

  UnstartedPagePaintObserver(const UnstartedPagePaintObserver&) = delete;
  UnstartedPagePaintObserver& operator=(const UnstartedPagePaintObserver&) =
      delete;

  ~UnstartedPagePaintObserver() override;

  // page_load_metrics::PageLoadMetricsObserver:
  const char* GetObserverName() const override;
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnEnterBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnRestoreFromBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing,
      content::NavigationHandle* navigation_handle) override;

 private:
  // Methods for handling unstarted page paints timer.
  void StartUnstartedPagePaintTimer();
  void StopUnstartedPagePaintTimer(bool first_content_paint);
  void OnUnstartedPagePaintExpired() const;

  // Timer to monitor that a navigation is completed in a certain amount of
  // time. The timer should be started whenever a navigation starts and stopped
  // upon navigation completed. In case that timer shots before completion, the
  // navigation will be reported as not completed. A navigation is considered
  // completed when a first contentful paint is received for the page.
  base::OneShotTimer navigation_timeout_timer_;

  base::WeakPtrFactory<UnstartedPagePaintObserver> weak_factory_{this};
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_CORE_UNSTARTED_PAGE_PAINT_OBSERVER_H_
