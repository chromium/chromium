// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"

#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "content/public/browser/render_frame_host.h"
#include "services/metrics/public/cpp/ukm_builders.h"

using Features = page_load_metrics::mojom::PageLoadFeatures;
using UkmFeatureList = UseCounterPageLoadMetricsObserver::UkmFeatureList;
using WebFeature = blink::mojom::WebFeature;
using WebFeatureBitSet =
    std::bitset<static_cast<size_t>(WebFeature::kNumberOfFeatures)>;

using CSSSampleId = blink::mojom::CSSSampleId;

namespace {

void RecordUkmFeatures(const UkmFeatureList& features,
                       const WebFeatureBitSet& features_recorded,
                       const WebFeatureBitSet& main_frame_features_recorded,
                       std::set<size_t>* ukm_features_recorded,
                       ukm::SourceId source_id) {
  for (auto feature : features) {
    if (!features_recorded.test(static_cast<size_t>(feature)))
      continue;
    if (ukm_features_recorded->find(static_cast<size_t>(feature)) !=
        ukm_features_recorded->end())
      continue;
    // TODO(kochi): https://crbug.com/806671 https://843080
    // as ElementCreateShadowRoot is ~8% and
    // DocumentRegisterElement is ~5% as of May, 2018, to meet UKM's data
    // volume expectation, reduce the data size by sampling. Revisit and
    // remove this code once Shadow DOM V0 and Custom Elements V0 are removed.
    const int kSamplingFactor = 10;
    if ((feature == WebFeature::kElementCreateShadowRoot ||
         feature == WebFeature::kDocumentRegisterElement) &&
        base::RandGenerator(kSamplingFactor) != 0)
      continue;

    ukm::builders::Blink_UseCounter(source_id)
        .SetFeature(static_cast<size_t>(feature))
        .SetIsMainFrameFeature(
            main_frame_features_recorded.test(static_cast<size_t>(feature)))
        .Record(ukm::UkmRecorder::Get());
    ukm_features_recorded->insert(static_cast<size_t>(feature));
  }
}

// It's always recommended to use the deprecation API in blink. If the feature
// was logged from the browser (or from both blink and the browser) where the
// deprecation API is not available, use this method for the console warning.
// Note that this doesn't generate the deprecation report.
void PossiblyWarnFeatureDeprecation(content::RenderFrameHost* rfh,
                                    WebFeature feature) {
  switch (feature) {
    case WebFeature::kDownloadInSandboxWithoutUserGesture:
      rfh->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kWarning,
          "[Deprecation] Download in sandbox without user activation is "
          "deprecated and will be removed in M76, around July 2019. You may "
          "consider adding "
          "'allow-downloads-without-user-activation' to the sandbox attribute "
          "list. See https://www.chromestatus.com/feature/5706745674465280 for "
          "more details.");
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
  ukm_features_recorded_.insert(static_cast<size_t>(WebFeature::kPageVisits));
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
    const Features& features) {
  for (WebFeature feature : features.features) {
    // Verify that kPageVisits is observed at most once per observer.
    if (feature == WebFeature::kPageVisits) {
      mojo::ReportBadMessage(
          "kPageVisits should not be passed to "
          "PageLoadMetricsObserver::OnFeaturesUsageObserved");
      return;
    }

    // Record feature usage in main frame.
    // If a feature is already recorded in the main frame, it is also recorded
    // on the page.
    if (main_frame_features_recorded_.test(static_cast<size_t>(feature)))
      continue;
    if (rfh->GetParent() == nullptr) {
      RecordMainFrameFeature(feature);
      main_frame_features_recorded_.set(static_cast<size_t>(feature));
    }

    if (features_recorded_.test(static_cast<size_t>(feature)))
      continue;
    PossiblyWarnFeatureDeprecation(rfh, feature);
    RecordFeature(feature);
    features_recorded_.set(static_cast<size_t>(feature));
  }

  for (CSSSampleId css_property : features.css_properties) {
    // Verify that page visit is observed at most once per observer.
    if (css_property == CSSSampleId::kTotalPagesMeasured) {
      mojo::ReportBadMessage(
          "CSSSampleId::kTotalPagesMeasured should not be passed to "
          "PageLoadMetricsObserver::OnFeaturesUsageObserved");
      return;
    }
    if (css_property > CSSSampleId::kMaxValue) {
      mojo::ReportBadMessage(
          "Invalid CSS property passed to "
          "PageLoadMetricsObserver::OnFeaturesUsageObserved");
      return;
    }
    // Same as above, the usage of each CSS property should be only measured
    // once.
    if (css_properties_recorded_.test(static_cast<size_t>(css_property)))
      continue;
    // There are about 600 enums, so the memory required for a vector histogram
    // is about 600 * 8 byes = 5KB
    // 50% of the time there are about 100 CSS properties recorded per page
    // load. Storage in sparce histogram entries are 48 bytes instead of 8
    // bytes so the memory required for a sparse histogram is about
    // 100 * 48 bytes = 5KB. On top there will be std::map overhead and the
    // acquire/release of a base::Lock to protect the map during each update.
    // Overal it is still better to use a vector histogram here since it is
    // faster to access and merge and uses about same amount of memory.
    RecordCssProperty(css_property);
    css_properties_recorded_.set(static_cast<size_t>(css_property));
  }

  for (CSSSampleId animated_css_property : features.animated_css_properties) {
    // Verify that page visit is observed at most once per observer.
    if (animated_css_property ==
        blink::mojom::CSSSampleId::kTotalPagesMeasured) {
      mojo::ReportBadMessage(
          "CSSSampleId::kTotalPagesMeasured should not be passed to "
          "PageLoadMetricsObserver::OnFeaturesUsageObserved");
      return;
    }
    if (animated_css_property > blink::mojom::CSSSampleId::kMaxValue) {
      mojo::ReportBadMessage(
          "Invalid animated CSS property passed to "
          "PageLoadMetricsObserver::OnFeaturesUsageObserved");
      return;
    }
    // Same as above, the usage of each animated CSS property should be only
    // measured once.
    if (animated_css_properties_recorded_.test(
            static_cast<size_t>(animated_css_property)))
      continue;
    // See comments above (in the css property section) for reasoning of using
    // a vector histogram here instead of a sparse histogram.
    RecordAnimatedCssProperty(animated_css_property);
    animated_css_properties_recorded_.set(
        static_cast<size_t>(animated_css_property));
  }
}

void UseCounterPageLoadMetricsObserver::OnComplete(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordUkmFeatures(GetAllowedUkmFeatures(), features_recorded_,
                    main_frame_features_recorded_, &ukm_features_recorded_,
                    GetDelegate().GetSourceId());
}

void UseCounterPageLoadMetricsObserver::OnFailedProvisionalLoad(
    const page_load_metrics::FailedProvisionalLoadInfo&
        failed_provisional_load_info) {
  RecordUkmFeatures(GetAllowedUkmFeatures(), features_recorded_,
                    main_frame_features_recorded_, &ukm_features_recorded_,
                    GetDelegate().GetSourceId());
}

page_load_metrics::PageLoadMetricsObserver::ObservePolicy
UseCounterPageLoadMetricsObserver::FlushMetricsOnAppEnterBackground(
    const page_load_metrics::mojom::PageLoadTiming& timing) {
  RecordUkmFeatures(GetAllowedUkmFeatures(), features_recorded_,
                    main_frame_features_recorded_, &ukm_features_recorded_,
                    GetDelegate().GetSourceId());
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
