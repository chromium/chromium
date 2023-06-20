// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRIVACY_SANDBOX_ADS_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRIVACY_SANDBOX_ADS_PAGE_LOAD_METRICS_OBSERVER_H_

#include <string>

#include "base/containers/enum_set.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom-forward.h"

namespace page_load_metrics {
class PrivacySandboxAdsPageLoadMetricsObserverTest;
}  // namespace page_load_metrics

class PrivacySandboxAdsPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  PrivacySandboxAdsPageLoadMetricsObserver() = default;
  ~PrivacySandboxAdsPageLoadMetricsObserver() override = default;

 private:
  friend class page_load_metrics::PrivacySandboxAdsPageLoadMetricsObserverTest;

  enum class PrivacySandboxAdsApi {
    kAttributionReporting,
    kFencedFrames,
    kProtectedAudienceRunAdAuction,
    kProtectedAudienceJoinAdInterestGroup,
    kPrivateAggregation,
    kSharedStorage,
    kTopics,

    kMinValue = kAttributionReporting,
    kMaxValue = kTopics,
  };

  static std::string GetHistogramName(const char* prefix, PrivacySandboxAdsApi);

  // page_load_metrics::PageLoadMetricsObserver:
  const char* GetObserverName() const override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  void OnFirstInputInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFirstContentfulPaintInPage(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  page_load_metrics::PageLoadMetricsObserver::ObservePolicy
  FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFeaturesUsageObserved(
      content::RenderFrameHost* rfh,
      const std::vector<blink::UseCounterFeature>& features) override;

  void RecordSessionEndHistograms(
      const page_load_metrics::mojom::PageLoadTiming& main_frame_timing);

  base::EnumSet<PrivacySandboxAdsApi,
                PrivacySandboxAdsApi::kMinValue,
                PrivacySandboxAdsApi::kMaxValue>
      used_privacy_sandbox_ads_apis_;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_PRIVACY_SANDBOX_ADS_PAGE_LOAD_METRICS_OBSERVER_H_
