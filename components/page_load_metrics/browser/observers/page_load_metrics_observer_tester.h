// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TESTER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TESTER_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/test/weak_mock_timer.h"
#include "components/ukm/test_ukm_recorder.h"
#include "net/cookies/canonical_cookie.h"
#include "ui/base/page_transition_types.h"

namespace base {
class GURL;
class HistogramTester;
}  // namespace base

namespace blink {
class WebInputEvent;
}  // namespace blink

namespace content {
class RenderFrameHost;
class RenderViewHostTestHarness;
class WebContents;
struct GlobalRequestID;
}  // namespace content

namespace mojom {
class FrameRenderDataUpdate;
class PageLoadFeatures;
class PageLoadMetadata;
class PageLoadTiming;
}  // namespace mojom

namespace ukm {
class TestAutoSetUkmRecorder;
}  // namespace ukm

namespace page_load_metrics {

class MetricsWebContentsObserver;
class PageLoadMetricsObserverDelegate;
class PageLoadTracker;
struct ExtraRequestCompleteInfo;

// This class creates a MetricsWebContentsObserver and provides methods for
// interacting with it. This class is designed to be used in unit tests for
// PageLoadMetricsObservers.
//
// To use it, simply instantiate it with a given WebContents, along with a
// callback used to register observers to a given PageLoadTracker.
class PageLoadMetricsObserverTester : public test::WeakMockTimerProvider {
 public:
  using RegisterObserversCallback =
      base::RepeatingCallback<void(PageLoadTracker*)>;
  PageLoadMetricsObserverTester(
      content::WebContents* web_contents,
      content::RenderViewHostTestHarness* rfh_test_harness,
      const RegisterObserversCallback& callback);
  ~PageLoadMetricsObserverTester() override;

  // Simulates starting a navigation to the given gurl, without committing the
  // navigation.
  // Note: The navigation is left in a pending state and cannot be successfully
  // completed.
  void StartNavigation(const GURL& gurl);

  // Simulates committing a navigation to the given URL with the given
  // PageTransition.
  void NavigateWithPageTransitionAndCommit(const GURL& url,
                                           ui::PageTransition transition);

  // Navigates to a URL that is not tracked by page_load_metrics. Useful for
  // forcing the OnComplete method of a PageLoadMetricsObserver to run.
  void NavigateToUntrackedUrl();

  // Call this to simulate sending a PageLoadTiming IPC from the render process
  // to the browser process. These will update the timing information for the
  // most recently committed navigation.
  void SimulateTimingUpdate(const mojom::PageLoadTiming& timing);
  void SimulateTimingUpdate(const mojom::PageLoadTiming& timing,
                            content::RenderFrameHost* rfh);
  void SimulateCpuTimingUpdate(const mojom::CpuTiming& cpu_timing);
  void SimulateCpuTimingUpdate(const mojom::CpuTiming& cpu_timing,
                               content::RenderFrameHost* rfh);
  void SimulateTimingAndMetadataUpdate(const mojom::PageLoadTiming& timing,
                                       const mojom::PageLoadMetadata& metadata);
  void SimulateMetadataUpdate(const mojom::PageLoadMetadata& metadata,
                              content::RenderFrameHost* rfh);
  void SimulateFeaturesUpdate(const mojom::PageLoadFeatures& new_features);
  void SimulateResourceDataUseUpdate(
      const std::vector<mojom::ResourceDataUpdatePtr>& resources);
  void SimulateResourceDataUseUpdate(
      const std::vector<mojom::ResourceDataUpdatePtr>& resources,
      content::RenderFrameHost* render_frame_host);
  void SimulateRenderDataUpdate(
      const mojom::FrameRenderDataUpdate& render_data);
  void SimulateRenderDataUpdate(const mojom::FrameRenderDataUpdate& render_data,
                                content::RenderFrameHost* render_frame_host);

  // Simulates a loaded resource. Main frame resources must specify a
  // GlobalRequestID, using the SimulateLoadedResource() method that takes a
  // |request_id| parameter.
  void SimulateLoadedResource(const ExtraRequestCompleteInfo& info);

  // Simulates a loaded resource, with the given GlobalRequestID.
  void SimulateLoadedResource(const ExtraRequestCompleteInfo& info,
                              const content::GlobalRequestID& request_id);

  // Simulate the first user interaction for a frame.
  void SimulateFrameReceivedFirstUserActivation(
      content::RenderFrameHost* render_frame_host);

  // Simulates a user input.
  void SimulateInputEvent(const blink::WebInputEvent& event);

  // Simulates the app being backgrounded.
  void SimulateAppEnterBackground();

  // Simulate playing a media element.
  void SimulateMediaPlayed();

  // Simulate reading cookies.
  void SimulateCookiesRead(const GURL& url,
                           const GURL& first_party_url,
                           const net::CookieList& cookie_list,
                           bool blocked_by_policy);

  // Simulate writing a cookie.
  void SimulateCookieChange(const GURL& url,
                            const GURL& first_party_url,
                            const net::CanonicalCookie& cookie,
                            bool blocked_by_policy);

  // Simulate accessing the local storage or session storage.
  void SimulateDomStorageAccess(const GURL& url,
                                const GURL& first_party_url,
                                bool local,
                                bool blocked_by_policy);

  MetricsWebContentsObserver* metrics_web_contents_observer() {
    return metrics_web_contents_observer_;
  }
  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }
  const ukm::TestAutoSetUkmRecorder& test_ukm_recorder() const {
    return test_ukm_recorder_;
  }
  const PageLoadMetricsObserverDelegate& GetDelegateForCommittedLoad() const;
  void RegisterObservers(PageLoadTracker* tracker);

 private:
  void SimulatePageLoadTimingUpdate(
      const mojom::PageLoadTiming& timing,
      const mojom::PageLoadMetadata& metadata,
      const mojom::PageLoadFeatures& new_features,
      const mojom::FrameRenderDataUpdate& render_data,
      const mojom::CpuTiming& cpu_timing,
      const mojom::DeferredResourceCounts& new_deferred_resource_data,
      content::RenderFrameHost* rfh);

  content::WebContents* web_contents() const { return web_contents_; }

  RegisterObserversCallback register_callback_;
  content::WebContents* web_contents_;
  content::RenderViewHostTestHarness* rfh_test_harness_;
  MetricsWebContentsObserver* metrics_web_contents_observer_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

  DISALLOW_COPY_AND_ASSIGN(PageLoadMetricsObserverTester);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TESTER_H_
