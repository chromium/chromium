// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_USE_COUNTER_PAGE_LOAD_METRICS_OBSERVER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_USE_COUNTER_PAGE_LOAD_METRICS_OBSERVER_H_

#include <bitset>
#include <string>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_functions.h"
#include "components/page_load_metrics/browser/observers/use_counter/at_most_once_enum_uma_deferrer.h"
#include "components/page_load_metrics/browser/page_load_metrics_observer.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/css_property_id.mojom.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "third_party/blink/public/mojom/use_counter/use_counter_feature.mojom-forward.h"

class UseCounterMetricsRecorder {
 public:
  // If `is_in_fenced_frames_page` is true, uses prefix
  // "Blink.UseCounter.FencedFrames.". Otherwise, uses
  // "Blink.UseCounter.".
  explicit UseCounterMetricsRecorder(bool is_in_fenced_frames_page);

  UseCounterMetricsRecorder(const UseCounterMetricsRecorder&) = delete;
  UseCounterMetricsRecorder& operator=(const UseCounterMetricsRecorder&) =
      delete;

  ~UseCounterMetricsRecorder();

  void AssertNoMetricsRecordedOrDeferred();

  // Records UKM for WebFeature::kPageVisits.
  void RecordUkmPageVisits(ukm::SourceId ukm_source_id);

  void DisableDeferAndFlush();

  // Records an `UseCounterFeature` through UMA_HISTOGRAM_ENUMERATION if
  // the feature has not been recorded before.
  void RecordOrDeferUseCounterFeature(content::RenderFrameHost*,
                                      const blink::UseCounterFeature&);

  // Records a WebFeature in main frame if `rfh` is a main frame and the feature
  // has not been recorded before.
  void RecordOrDeferMainFrameWebFeature(content::RenderFrameHost*,
                                        blink::mojom::WebFeature);

  // Records UKM subset of WebFeatures, if the WebFeature is observed in the
  // page.
  void RecordUkmFeatures(ukm::SourceId ukm_source_id);

  using UkmFeatureList = base::flat_set<blink::mojom::WebFeature>;

 private:
  // Returns a list of opt-in UKM features for use counter.
  static const UkmFeatureList& GetAllowedUkmFeatures();

  // Returns a list of opt-in UKM features for the Web Dev Metrics use counter.
  static const UkmFeatureList& GetAllowedWebDevMetricsUkmFeatures();

  // To keep tracks of which features have been measured.
  AtMostOnceEnumUmaDeferrer<blink::mojom::WebFeature> uma_features_;
  AtMostOnceEnumUmaDeferrer<blink::mojom::WebFeature> uma_main_frame_features_;
  AtMostOnceEnumUmaDeferrer<blink::mojom::CSSSampleId> uma_css_properties_;
  AtMostOnceEnumUmaDeferrer<blink::mojom::CSSSampleId>
      uma_animated_css_properties_;

  AtMostOnceEnumUmaDeferrer<blink::mojom::PermissionsPolicyFeature>
      uma_permissions_policy_violation_enforce_;

  AtMostOnceEnumUmaDeferrer<blink::mojom::PermissionsPolicyFeature>
      uma_permissions_policy_allow2_;

  AtMostOnceEnumUmaDeferrer<blink::mojom::PermissionsPolicyFeature>
      uma_permissions_policy_header2_;
  AtMostOnceEnumUmaDeferrer<
      blink::UserAgentOverride::UserAgentOverrideHistogram>
      uma_user_agent_override_;

  // To keep tracks of which features have been measured.
  std::bitset<static_cast<size_t>(blink::mojom::WebFeature::kNumberOfFeatures)>
      ukm_features_recorded_;
  std::bitset<static_cast<size_t>(blink::mojom::WebFeature::kNumberOfFeatures)>
      webdev_metrics_ukm_features_recorded_;
};

// This class reports several use counters coming from Blink.
// For FencedFrames, it reports the use counters with a "FencedFrames" prefix.
class UseCounterPageLoadMetricsObserver
    : public page_load_metrics::PageLoadMetricsObserver {
 public:
  UseCounterPageLoadMetricsObserver();

  UseCounterPageLoadMetricsObserver(const UseCounterPageLoadMetricsObserver&) =
      delete;
  UseCounterPageLoadMetricsObserver& operator=(
      const UseCounterPageLoadMetricsObserver&) = delete;

  ~UseCounterPageLoadMetricsObserver() override;

  // page_load_metrics::PageLoadMetricsObserver.
  ObservePolicy OnStart(content::NavigationHandle* navigation_handle,
                        const GURL& currently_committed_url,
                        bool started_in_foreground) override;
  ObservePolicy OnFencedFramesStart(
      content::NavigationHandle* navigation_handle,
      const GURL& currently_committed_url) override;
  ObservePolicy OnPrerenderStart(content::NavigationHandle* navigation_handle,
                                 const GURL& currently_committed_url) override;
  ObservePolicy OnCommit(content::NavigationHandle* navigation_handle) override;
  void DidActivatePrerenderedPage(
      content::NavigationHandle* navigation_handle) override;
  void OnFeaturesUsageObserved(
      content::RenderFrameHost* rfh,
      const std::vector<blink::UseCounterFeature>&) override;
  void OnComplete(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  void OnFailedProvisionalLoad(
      const page_load_metrics::FailedProvisionalLoadInfo&
          failed_provisional_load_info) override;
  ObservePolicy OnEnterBackForwardCache(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy FlushMetricsOnAppEnterBackground(
      const page_load_metrics::mojom::PageLoadTiming& timing) override;
  ObservePolicy ShouldObserveMimeType(
      const std::string& mime_type) const override;

 private:
  bool IsInPrerenderingBeforeActivation() const;

  std::unique_ptr<UseCounterMetricsRecorder> recorder_;
};

#endif  // COMPONENTS_PAGE_LOAD_METRICS_BROWSER_OBSERVERS_USE_COUNTER_PAGE_LOAD_METRICS_OBSERVER_H_
