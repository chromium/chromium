// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_USE_COUNTER_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_USE_COUNTER_PAGE_LOAD_METRICS_OBSERVER_H_

#include <bitset>
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "third_party/blink/public/mojom/use_counter/css_property_id.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"

namespace internal {

const char kFeaturesHistogramName[] = "Blink.UseCounter.Features";
const char kFeaturesHistogramMainFrameName[] =
    "Blink.UseCounter.MainFrame.Features";
const char kCssPropertiesHistogramName[] = "Blink.UseCounter.CSSProperties";
const char kAnimatedCssPropertiesHistogramName[] =
    "Blink.UseCounter.AnimatedCSSProperties";

}  // namespace internal

class UseCounterPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  UseCounterPageLoadMetricsObserver();
  ~UseCounterPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver.
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle,
                         ukm::SourceId source_id) override;
  void OnFeaturesUsageObserved(
      content::RenderFrameHost* rfh,
      const page_load_metrics::mojom::PageLoadFeatures&) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo&
          failed_provisional_load_info) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const override;

  using UkmFeatureList = base::flat_set<blink::mojom::WebFeature>;

 private:
  // Returns a list of opt-in UKM features for use counter.
  static const UkmFeatureList& GetAllowedUkmFeatures();

  // To keep tracks of which features have been measured.
  std::bitset<static_cast<size_t>(blink::mojom::WebFeature::kNumberOfFeatures)>
      features_recorded_;
  std::bitset<static_cast<size_t>(blink::mojom::WebFeature::kNumberOfFeatures)>
      main_frame_features_recorded_;
  std::bitset<static_cast<size_t>(blink::mojom::CSSSampleId::kMaxValue) + 1>
      css_properties_recorded_;
  std::bitset<static_cast<size_t>(blink::mojom::CSSSampleId::kMaxValue) + 1>
      animated_css_properties_recorded_;
  std::set<size_t> ukm_features_recorded_;
  DISALLOW_COPY_AND_ASSIGN(UseCounterPageLoadMetricsObserver);
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_USE_COUNTER_PAGE_LOAD_METRICS_OBSERVER_H_
