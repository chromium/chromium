// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TESTER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TESTER_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom-forward.h"
#include "components/page_load_metrics/common/test/weak_mock_timer.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/cookie_access_details.h"
#include "net/cookies/canonical_cookie.h"
#include "ui/base/page_transition_types.h"

namespace base {
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
class NavigationHandle;
}  // namespace content

class GURL;

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
      const RegisterObserversCallback& callback,
      bool is_non_tab_webui = false);

  PageLoadMetricsObserverTester(const PageLoadMetricsObserverTester&) = delete;
  PageLoadMetricsObserverTester& operator=(
      const PageLoadMetricsObserverTester&) = delete;

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
  void SimulateInputTimingUpdate(const mojom::InputTiming& input_timing);
  void SimulateInputTimingUpdate(const mojom::InputTiming& input_timing,
                                 content::RenderFrameHost* rfh);
  void SimulateTimingAndMetadataUpdate(const mojom::PageLoadTiming& timing,
                                       const mojom::FrameMetadata& metadata);
  void SimulateMetadataUpdate(const mojom::FrameMetadata& metadata,
                              content::RenderFrameHost* rfh);
  void SimulateFeaturesUpdate(
      const std::vector<blink::UseCounterFeature>& new_features);
  void SimulateResourceDataUseUpdate(
      const std::vector<mojom::ResourceDataUpdatePtr>& resources);
  void SimulateResourceDataUseUpdate(
      const std::vector<mojom::ResourceDataUpdatePtr>& resources,
      content::RenderFrameHost* render_frame_host);
  void SimulateRenderDataUpdate(
      const mojom::FrameRenderDataUpdate& render_data);
  void SimulateRenderDataUpdate(const mojom::FrameRenderDataUpdate& render_data,
                                content::RenderFrameHost* render_frame_host);
  void SimulateSoftNavigation(content::NavigationHandle* navigation_handle);
  void SimulateDidFinishNavigation(
      content::NavigationHandle* navigation_handle);
  void SimulateSoftNavigationCountUpdate(
      const mojom::SoftNavigationMetrics& soft_navigation_metrics);
  void SimulateCustomUserTimingUpdate(mojom::CustomUserTimingMarkPtr timing);

  // Simulates a loaded resource. Main frame resources must specify a
  // GlobalRequestID, using the SimulateLoadedResource() method that takes a
  // |request_id| parameter.
  void SimulateLoadedResource(const ExtraRequestCompleteInfo& info);

  // Simulates a loaded resource, with the given GlobalRequestID.
  void SimulateLoadedResource(const ExtraRequestCompleteInfo& info,
                              const content::GlobalRequestID& request_id);

  // Simulate the user interaction for a frame.
  void SimulateFrameReceivedUserActivation(
      content::RenderFrameHost* render_frame_host);

  // Simulates a user input.
  void SimulateInputEvent(const blink::WebInputEvent& event);

  // Simulates the app being backgrounded.
  void SimulateAppEnterBackground();

  // Simulate playing a media element.
  void SimulateMediaPlayed();
  void SimulateMediaPlayed(content::RenderFrameHost* rfh);

  // Simulate accessingcookies.
  void SimulateCookieAccess(const content::CookieAccessDetails& details);

  // Simulate accessing the local storage or session storage.
  void SimulateStorageAccess(const GURL& url,
                             const GURL& first_party_url,
                             bool blocked_by_policy,
                             StorageType storage_type);

  // Simulate a V8 per-frame memory update.
  void SimulateMemoryUpdate(content::RenderFrameHost* render_frame_host,
                            int64_t delta_bytes);

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

  bool is_non_tab_webui() const { return is_non_tab_webui_; }

 private:
  void SimulatePageLoadTimingUpdate(
      const mojom::PageLoadTiming& timing,
      const mojom::FrameMetadata& metadata,
      const std::vector<blink::UseCounterFeature>& new_features,
      const mojom::FrameRenderDataUpdate& render_data,
      const mojom::CpuTiming& cpu_timing,
      const mojom::InputTiming& input_timing,
      const std::optional<blink::SubresourceLoadMetrics>&
          subresource_load_metrics,
      content::RenderFrameHost* rfh,
      const mojom::SoftNavigationMetrics& soft_navigation_metrics);

  content::WebContents* web_contents() const { return web_contents_; }

  RegisterObserversCallback register_callback_;
  raw_ptr<content::WebContents, DanglingUntriaged> web_contents_;
  raw_ptr<content::RenderViewHostTestHarness> rfh_test_harness_;
  raw_ptr<MetricsWebContentsObserver, DanglingUntriaged>
      metrics_web_contents_observer_;
  base::HistogramTester histogram_tester_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;

  bool is_non_tab_webui_ = false;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PAGE_LOAD_METRICS_OBSERVER_TESTER_H_
