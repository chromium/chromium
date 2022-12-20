// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"

#include "base/no_destructor.h"

// This file defines a list of UseCounter WebFeature measured in the
// Web Developer Metric UKM UseCounter. Features must all satisfy UKM
// privacy requirements (see go/ukm). In addition, features should only be added
// if the expected frequency is less than 5% of page loads.
//
// The Web Developer Metrics UKM UseCounter is intended for features that are
// launching or may be subject to spec revisions. The UKM data is used to tie
// feature usage by a site to performance, devtools usage or other aspects of
// the site's structure. The event may also be used for investigations into
// features with unknown or unexpected UseCounter data in order to identify
// sites for further attention.
//
// Do NOT use this list for features that are being considered for removal or
// deprecation where percentage of page views or URLs is important. This event
// may be subsampled by the UKM system (some proportion of events may be dropped
// before being uploaded from the client) in order to balance the resources of
// the UKM system at large. As a result, any feature usage counts or
// percentages aggregated across all page views or URLs will be inaccurate.
// For such cases use the Blink.UseCounter allow list found in
// components/page_load_metrics/browser/observers/use_counter/ukm_features.cc.

using WebFeature = blink::mojom::WebFeature;

// UKM-based UseCounter features (WebFeature) should be defined in
// opt_in_features list.
const UseCounterMetricsRecorder::UkmFeatureList&
UseCounterMetricsRecorder::GetAllowedWebDevMetricsUkmFeatures() {
  static base::NoDestructor<UseCounterMetricsRecorder::UkmFeatureList>
      // We explicitly use an std::initializer_list below to work around GCC
      // bug 84849, which causes having a base::NoDestructor<T<U>> and passing
      // an initializer list of Us does not work.
      opt_in_features(std::initializer_list<WebFeature>(
          {WebFeature::kCSSCascadeLayers,
           WebFeature::kFontSelectorCSSFontFamilyWebKitPrefixBody,
           WebFeature::kFontBuilderCSSFontFamilyWebKitPrefixBody,
           WebFeature::kScrollTimelineConstructor,
           WebFeature::kCSSAtRuleScrollTimeline,
           WebFeature::kCSSAtRuleContainer,
           WebFeature::kBlockingAttributeRenderToken,
           WebFeature::kV8MemoryInfo_TotalJSHeapSize_AttributeGetter,
           WebFeature::kV8MemoryInfo_UsedJSHeapSize_AttributeGetter,
           WebFeature::kV8MemoryInfo_JSHeapSizeLimit_AttributeGetter,
           WebFeature::kWindowOpenPopupOnMobile,
           WebFeature::kWindowOpenedAsPopupOnMobile}));
  return *opt_in_features;
}
