// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PAID_CONTENT_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PAID_CONTENT_PAGE_LOAD_METRICS_OBSERVER_H_

#include <optional>

#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/content_extraction/frame_metadata_observer_registry.mojom.h"

namespace content {
class NavigationHandle;
}

// Observer that tracks user interactions with pages that have paid content.
class PaidContentPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver,
      public blink::mojom::PaidContentMetadataObserver {
 public:
  PaidContentPageLoadMetricsObserver();
  ~PaidContentPageLoadMetricsObserver() override;

  PaidContentPageLoadMetricsObserver(
      const PaidContentPageLoadMetricsObserver&) = delete;
  PaidContentPageLoadMetricsObserver& operator=(
      const PaidContentPageLoadMetricsObserver&) = delete;

  // page_load_metrics::PageLoadMetricsObserver:
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;

  // blink::mojom::PaidContentMetadataObserver:
  void OnPaidContentMetadataChanged(bool has_paid_content) override;

 private:
  // Called when the connection to the renderer is disconnected.
  void OnPaidContentMetadataObserverDisconnect();

  // Set when the page has paid content.
  std::optional<bool> has_paid_content_;

  mojo::Remote<blink::mojom::FrameMetadataObserverRegistry> registry_;
  mojo::Receiver<blink::mojom::PaidContentMetadataObserver> receiver_{this};
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PAID_CONTENT_PAGE_LOAD_METRICS_OBSERVER_H_
