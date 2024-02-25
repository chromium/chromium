// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/privacy_sandbox_ads_page_load_metrics_observer.h"

#include <optional>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/page_load_metrics/browser/observers/core/largest_contentful_paint_handler.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_delegate.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer_interface.h"
#include "components/page_load_metrics/browser/page_load_metrics_util.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom.h"

namespace {

using FeatureType = blink::mojom::UseCounterFeatureType;
using WebFeature = blink::mojom::WebFeature;

constexpr char kHistogramPrivacySandboxAdsFirstInputDelay4Prefix[] =
    "PageLoad.Clients.PrivacySandboxAds.InteractiveTiming.FirstInputDelay4.";

constexpr char
    kHistogramPrivacySandboxAdsNavigationToFirstContentfulPaintPrefix[] =
        "PageLoad.Clients.PrivacySandboxAds.PaintTiming."
        "NavigationToFirstContentfulPaint.";

constexpr char
    kHistogramPrivacySandboxAdsNavigationToLargestContentfulPaint2Prefix[] =
        "PageLoad.Clients.PrivacySandboxAds.PaintTiming."
        "NavigationToLargestContentfulPaint2.";

constexpr char kHistogramPrivacySandboxAdsMaxCumulativeShiftScorePrefix[] =
    "PageLoad.Clients.PrivacySandboxAds.LayoutInstability."
    "MaxCumulativeShiftScore.SessionWindow.Gap1000ms.Max5000ms2.";

}  // namespace

// static
std::string PrivacySandboxAdsPageLoadMetricsObserver::GetHistogramName(
    const char* prefix,
    PrivacySandboxAdsApi api) {
  const char* suffix;
  switch (api) {
    case PrivacySandboxAdsApi::kAttributionReporting:
      suffix = "AttributionReporting";
      break;
    case PrivacySandboxAdsApi::kFencedFrames:
      suffix = "FencedFrames";
      break;
    case PrivacySandboxAdsApi::kProtectedAudienceRunAdAuction:
      suffix = "ProtectedAudienceRunAdAuction";
      break;
    case PrivacySandboxAdsApi::kProtectedAudienceJoinAdInterestGroup:
      suffix = "ProtectedAudienceJoinAdInterestGroup";
      break;
    case PrivacySandboxAdsApi::kPrivateAggregation:
      suffix = "PrivateAggregation";
      break;
    case PrivacySandboxAdsApi::kSharedStorage:
      suffix = "SharedStorage";
      break;
    case PrivacySandboxAdsApi::kTopics:
      suffix = "Topics";
      break;
  }
  return base::StrCat({prefix, suffix});
}

const char* PrivacySandboxAdsPageLoadMetricsObserver::GetObserverName() const {
  static const char kName[] = "PrivacySandboxAdsPageLoadMetricsObserver";
  return kName;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrivacySandboxAdsPageLoadMetricsObserver::OnFencedFramesStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  // `OnFeaturesUsageObserved()` needs observer level forwarding.
  return FORWARD_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrivacySandboxAdsPageLoadMetricsObserver::OnPrerenderStart(
    content::NavigationHandle* navigation_handle,
    const GURL& currently_committed_url) {
  return CONTINUE_OBSERVING;
}

void PrivacySandboxAdsPageLoadMetricsObserver::OnFirstInputInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.interactive_timing->first_input_timestamp, GetDelegate())) {
    return;
  }

  for (PrivacySandboxAdsApi api : used_privacy_sandbox_ads_apis_) {
    base::UmaHistogramCustomTimes(
        GetHistogramName(kHistogramPrivacySandboxAdsFirstInputDelay4Prefix,
                         api),
        timing.interactive_timing->first_input_delay.value(),
        base::Milliseconds(1), base::Seconds(60), 50);
  }
}

void PrivacySandboxAdsPageLoadMetricsObserver::OnFirstContentfulPaintInPage(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  if (!page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          timing.paint_timing->first_contentful_paint, GetDelegate())) {
    return;
  }

  for (PrivacySandboxAdsApi api : used_privacy_sandbox_ads_apis_) {
    base::UmaHistogramCustomTimes(
        GetHistogramName(
            kHistogramPrivacySandboxAdsNavigationToFirstContentfulPaintPrefix,
            api),
        timing.paint_timing->first_contentful_paint.value(),
        base::Milliseconds(10), base::Minutes(10), 100);
  }
}

void PrivacySandboxAdsPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms(timing);
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
PrivacySandboxAdsPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordSessionEndHistograms(timing);
  return STOP_OBSERVING;
}

void PrivacySandboxAdsPageLoadMetricsObserver::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const std::vector<blink::UseCounterFeature>& features) {
  for (const blink::UseCounterFeature& feature : features) {
    if (feature.type() != FeatureType::kWebFeature) {
      continue;
    }

    std::optional<PrivacySandboxAdsApi> api;
    switch (static_cast<WebFeature>(feature.value())) {
      case WebFeature::kAttributionReportingAPIAll:
        api = PrivacySandboxAdsApi::kAttributionReporting;
        break;
      case WebFeature::kHTMLFencedFrameElement:
        api = PrivacySandboxAdsApi::kFencedFrames;
        break;
      case WebFeature::kV8Navigator_RunAdAuction_Method:
        api = PrivacySandboxAdsApi::kProtectedAudienceRunAdAuction;
        break;
      case WebFeature::kV8Navigator_JoinAdInterestGroup_Method:
        api = PrivacySandboxAdsApi::kProtectedAudienceJoinAdInterestGroup;
        break;
      case WebFeature::kPrivateAggregationApiAll:
        api = PrivacySandboxAdsApi::kPrivateAggregation;
        break;
      case WebFeature::kSharedStorageAPI_SharedStorage_DOMReference:
      case WebFeature::kSharedStorageAPI_Run_Method:
      case WebFeature::kSharedStorageAPI_SelectURL_Method:
        api = PrivacySandboxAdsApi::kSharedStorage;
        break;
      case WebFeature::kTopicsAPI_BrowsingTopics_Method:
        api = PrivacySandboxAdsApi::kTopics;
        break;
      default:
        break;
    }
    if (api.has_value()) {
      used_privacy_sandbox_ads_apis_.Put(*api);
    }
  }
}

void PrivacySandboxAdsPageLoadMetricsObserver::RecordSessionEndHistograms(
    const page_load_metrics::mojom::PageLoadTiming& main_frame_timing) {
  if (!GetDelegate().DidCommit()) {
    return;
  }

  const page_load_metrics::ContentfulPaintTimingInfo& largest_contentful_paint =
      GetDelegate()
          .GetLargestContentfulPaintHandler()
          .MergeMainFrameAndSubframes();
  if (largest_contentful_paint.ContainsValidTime() &&
      page_load_metrics::WasStartedInForegroundOptionalEventInForeground(
          largest_contentful_paint.Time(), GetDelegate())) {
    for (PrivacySandboxAdsApi api : used_privacy_sandbox_ads_apis_) {
      base::UmaHistogramCustomTimes(
          GetHistogramName(
              kHistogramPrivacySandboxAdsNavigationToLargestContentfulPaint2Prefix,
              api),
          largest_contentful_paint.Time().value(), base::Milliseconds(10),
          base::Minutes(10), 100);
    }
  }

  const page_load_metrics::NormalizedCLSData& normalized_cls_data =
      GetDelegate().GetNormalizedCLSData(
          page_load_metrics::PageLoadMetricsObserverDelegate::BfcacheStrategy::
              ACCUMULATE);
  if (!normalized_cls_data.data_tainted) {
    for (PrivacySandboxAdsApi api : used_privacy_sandbox_ads_apis_) {
      page_load_metrics::UmaMaxCumulativeShiftScoreHistogram10000x(
          GetHistogramName(
              kHistogramPrivacySandboxAdsMaxCumulativeShiftScorePrefix, api),
          normalized_cls_data);
    }
  }
}
