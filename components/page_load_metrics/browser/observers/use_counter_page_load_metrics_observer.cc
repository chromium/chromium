// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "content/public/browser/render_frame_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using UkmFeatureList = UseCounterPageLoadMetricsObserver::UkmFeatureList;
using WebFeature = blink::mojom::WebFeature;
using WebFeatureBitSet =
    std::bitset<static_cast<size_t>(WebFeature::kNumberOfFeatures)>;

using CSSSampleId = blink::mojom::CSSSampleId;

namespace {

// It's always recommended to use the deprecation API in blink. If the feature
// was logged from the browser (or from both blink and the browser) where the
// deprecation API is not available, use this method for the console warning.
// Note that this doesn't generate the deprecation report.
void PossiblyWarnFeatureDeprecation(content::RenderFrameHost* rfh,
                                    WebFeature feature) {
  switch (feature) {
    case WebFeature::kDownloadInSandbox:
      rfh->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "Download is disallowed. The frame initiating or instantiating the "
          "download is sandboxed, but the flag ‘allow-downloads’ is not set. "
          "See https://www.chromestatus.com/feature/5706745674465280 for more "
          "details.");
      return;
    case WebFeature::kDownloadInAdFrameWithoutUserGesture:
      rfh->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "[Intervention] Download in ad frame without user activation is "
          "not allowed. See "
          "https://www.chromestatus.com/feature/6311883621531648 for more "
          "details.");
      return;

    default:
      return;
  }
}

void RecordMainFrameFeature(blink::mojom::WebFeature feature) {
  UMA_HISTOGRAM_ENUMERATION(internal::kFeaturesHistogramMainFrameName, feature);
}

void RecordFeature(blink::mojom::WebFeature feature) {
  UMA_HISTOGRAM_ENUMERATION(internal::kFeaturesHistogramName, feature);
}

void RecordCssProperty(CSSSampleId property) {
  UMA_HISTOGRAM_ENUMERATION(internal::kCssPropertiesHistogramName, property);
}

void RecordAnimatedCssProperty(CSSSampleId animated_property) {
  UMA_HISTOGRAM_ENUMERATION(internal::kAnimatedCssPropertiesHistogramName,
                            animated_property);
}

}  // namespace

UseCounterPageLoadMetricsObserver::UseCounterPageLoadMetricsObserver() =
    default;

UseCounterPageLoadMetricsObserver::~UseCounterPageLoadMetricsObserver() =
    default;

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnCommit(
    content::NavigationHandle* navigation_handle,
    ukm::SourceId source_id) {
  // Verify that no feature usage is observed before commit
  DCHECK_LE(features_recorded_.count(), 0ul);
  DCHECK_LE(main_frame_features_recorded_.count(), 0ul);

  ukm::builders::Blink_UseCounter(source_id)
      .SetFeature(static_cast<size_t>(WebFeature::kPageVisits))
      .SetIsMainFrameFeature(1)
      .Record(ukm::UkmRecorder::Get());
  ukm_features_recorded_.set(static_cast<size_t>(WebFeature::kPageVisits));
  RecordFeature(WebFeature::kPageVisits);
  RecordMainFrameFeature(WebFeature::kPageVisits);
  RecordCssProperty(CSSSampleId::kTotalPagesMeasured);
  RecordAnimatedCssProperty(CSSSampleId::kTotalPagesMeasured);
  features_recorded_.set(static_cast<size_t>(WebFeature::kPageVisits));
  main_frame_features_recorded_.set(
      static_cast<size_t>(WebFeature::kPageVisits));
  return CONTINUE_OBSERVING;
}

void UseCounterPageLoadMetricsObserver::OnFeaturesUsageObserved(
    content::RenderFrameHost* rfh,
    const std::vector<blink::UseCounterFeature>& features) {
  using FeatureType = blink::mojom::UseCounterFeatureType;
  for (const blink::UseCounterFeature& feature : features) {
    switch (feature.type()) {
      case FeatureType::kWebFeature: {
        WebFeature web_feature = static_cast<WebFeature>(feature.value());
        // Record feature usage in main frame.
        // If a feature is already recorded in the main frame, it is also
        // recorded on the page.
        if (main_frame_features_recorded_.test(feature.value()))
          continue;
        if (rfh->GetParent() == nullptr) {
          RecordMainFrameFeature(web_feature);
          main_frame_features_recorded_.set(feature.value());
        }

        if (features_recorded_.test(feature.value()))
          continue;
        PossiblyWarnFeatureDeprecation(rfh, web_feature);
        RecordFeature(web_feature);
        features_recorded_.set(feature.value());
        break;
      }
      case FeatureType::kCssProperty: {
        CSSSampleId css_property = static_cast<CSSSampleId>(feature.value());
        // Same as above, the usage of each CSS property should be only measured
        // once.
        if (css_properties_recorded_.test(feature.value()))
          continue;
        // There are about 600 enums, so the memory required for a vector
        // histogram is about 600 * 8 byes = 5KB 50% of the time there are about
        // 100 CSS properties recorded per page load. Storage in sparce
        // histogram entries are 48 bytes instead of 8 bytes so the memory
        // required for a sparse histogram is about 100 * 48 bytes = 5KB. On top
        // there will be std::map overhead and the acquire/release of a
        // base::Lock to protect the map during each update. Overal it is still
        // better to use a vector histogram here since it is faster to access
        // and merge and uses about same amount of memory.
        RecordCssProperty(css_property);
        css_properties_recorded_.set(feature.value());
        break;
      }
      case FeatureType::kAnimatedCssProperty: {
        CSSSampleId animated_css_property =
            static_cast<CSSSampleId>(feature.value());
        // Same as above, the usage of each animated CSS property should be only
        // measured once.
        if (animated_css_properties_recorded_.test(feature.value()))
          continue;
        // See comments above (in the css property section) for reasoning of
        // using a vector histogram here instead of a sparse histogram.
        RecordAnimatedCssProperty(animated_css_property);
        animated_css_properties_recorded_.set(feature.value());
        break;
      }
    }
  }
}

void UseCounterPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordUkmFeatures();
}

void UseCounterPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo&
        failed_provisional_load_info) {
  RecordUkmFeatures();
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordUkmFeatures();
  return CONTINUE_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::ShouldObserveMimeType(
    const std::string& mime_type) const {
  return PageLoadMetricsObserver::ShouldObserveMimeType(mime_type) ==
                     CONTINUE_OBSERVING ||
                 mime_type == "image/svg+xml"
             ? CONTINUE_OBSERVING
             : STOP_OBSERVING;
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::OnEnterBackForwardCache(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  return CONTINUE_OBSERVING;
}

void UseCounterPageLoadMetricsObserver::RecordUkmFeatures() {
  for (auto feature : GetAllowedUkmFeatures()) {
    if (!features_recorded_.test(static_cast<size_t>(feature)))
      continue;
    if (ukm_features_recorded_.test(static_cast<size_t>(feature)))
      continue;
    ukm_features_recorded_.set(static_cast<size_t>(feature));

    ukm::builders::Blink_UseCounter(GetDelegate().GetPageUkmSourceId())
        .SetFeature(static_cast<size_t>(feature))
        .SetIsMainFrameFeature(
            main_frame_features_recorded_.test(static_cast<size_t>(feature)))
        .Record(ukm::UkmRecorder::Get());
  }
}
