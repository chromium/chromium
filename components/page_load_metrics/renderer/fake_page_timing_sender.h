// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_FAKE_PAGE_TIMING_SENDER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_FAKE_PAGE_TIMING_SENDER_H_

#include <set>
#include <vector>

#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/renderer/page_timing_sender.h"

namespace page_load_metrics {

// PageTimingSender implementation for use in tests. Allows for setting and
// verifying basic expectations when sending PageLoadTiming. By default,
// FakePageTimingSender will verify that expected and actual
// PageLoadTimings match on each invocation to ExpectPageLoadTiming() and
// SendTiming(), as well as in the destructor. Tests can force additional
// validations by calling VerifyExpectedTimings.
//
// Expected PageLoadTimings are specified via ExpectPageLoadTiming, and actual
// PageLoadTimings are dispatched through SendTiming(). When SendTiming() is
// called, we verify that the actual PageLoadTimings dipatched through
// SendTiming() match the expected PageLoadTimings provided via
// ExpectPageLoadTiming.
//
// Normally, gmock would be used in place of this class, but gmock is not
// compatible with structures that use aligned memory, and PageLoadTiming uses
// base::Optional which uses aligned memory, so we're forced to roll
// our own implementation here. See
// https://groups.google.com/forum/#!topic/googletestframework/W-Hud3j_c6I for
// more details.
class FakePageTimingSender : public PageTimingSender {
 public:
  class PageTimingValidator {
   public:
    PageTimingValidator();
    ~PageTimingValidator();
    // PageLoadTimings that are expected to be sent through SendTiming() should
    // be passed to ExpectPageLoadTiming.
    void ExpectPageLoadTiming(const mojom::PageLoadTiming& timing);

    // CpuTimings that are expected to be sent through SendTiming() should be
    // passed to ExpectCpuTiming.
    void ExpectCpuTiming(const base::TimeDelta& timing);

    // Forces verification that actual timings sent through SendTiming() match
    // expected timings provided via ExpectPageLoadTiming.
    void VerifyExpectedTimings() const;

    // Forces verification that actual timings sent through SendTiming() match
    // expected timings provided via ExpectCpuTiming.
    void VerifyExpectedCpuTimings() const;

    void VerifyExpectedInputTiming() const;

    // PageLoad features that are expected to be sent through SendTiming()
    // should be passed via UpdateExpectedPageLoadFeatures.
    void UpdateExpectPageLoadFeatures(const blink::mojom::WebFeature feature);
    // PageLoad CSS properties that are expected to be sent through SendTiming()
    // should be passed via UpdateExpectedPageLoadCSSProperties.
    void UpdateExpectPageLoadCssProperties(
        blink::mojom::CSSSampleId css_property_id);

    void UpdateExpectFrameRenderDataUpdate(
        const mojom::FrameRenderDataUpdate& render_data) {
      expected_render_data_ = render_data;
    }

    void UpdateExpectedInputTiming(const base::TimeDelta input_delay);

    void UpdateExpectFrameIntersectionUpdate(
        const mojom::FrameIntersectionUpdate& frame_intersection_update) {
      expected_frame_intersection_update_ = frame_intersection_update.Clone();
    }

    // Forces verification that actual features sent through SendTiming match
    // expected features provided via ExpectPageLoadFeatures.
    void VerifyExpectedFeatures() const;
    // Forces verification that actual CSS properties sent through SendTiming
    // match expected CSS properties provided via ExpectPageLoadCSSProperties.
    void VerifyExpectedCssProperties() const;
    void VerifyExpectedRenderData() const;
    void VerifyExpectedFrameIntersectionUpdate() const;

    const std::vector<mojom::PageLoadTimingPtr>& expected_timings() const {
      return expected_timings_;
    }
    const std::vector<mojom::PageLoadTimingPtr>& actual_timings() const {
      return actual_timings_;
    }

    void UpdateTiming(
        const mojom::PageLoadTimingPtr& timing,
        const mojom::FrameMetadataPtr& metadata,
        const mojom::PageLoadFeaturesPtr& new_features,
        const std::vector<mojom::ResourceDataUpdatePtr>& resources,
        const mojom::FrameRenderDataUpdate& render_data,
        const mojom::CpuTimingPtr& cpu_timing,
        const mojom::DeferredResourceCountsPtr& new_deferred_resource_data,
        const mojom::InputTimingPtr& input_timing);

   private:
    std::vector<mojom::PageLoadTimingPtr> expected_timings_;
    std::vector<mojom::PageLoadTimingPtr> actual_timings_;
    std::vector<mojom::CpuTimingPtr> expected_cpu_timings_;
    std::vector<mojom::CpuTimingPtr> actual_cpu_timings_;
    std::set<blink::mojom::WebFeature> expected_features_;
    std::set<blink::mojom::WebFeature> actual_features_;
    std::set<blink::mojom::CSSSampleId> expected_css_properties_;
    std::set<blink::mojom::CSSSampleId> actual_css_properties_;
    mojom::FrameRenderDataUpdate expected_render_data_;
    mojom::FrameRenderDataUpdate actual_render_data_;
    mojom::FrameIntersectionUpdatePtr expected_frame_intersection_update_;
    mojom::FrameIntersectionUpdatePtr actual_frame_intersection_update_;
    mojom::InputTimingPtr expected_input_timing;
    mojom::InputTimingPtr actual_input_timing;
    DISALLOW_COPY_AND_ASSIGN(PageTimingValidator);
  };

  explicit FakePageTimingSender(PageTimingValidator* validator);
  ~FakePageTimingSender() override;
  void SendTiming(const mojom::PageLoadTimingPtr& timing,
                  const mojom::FrameMetadataPtr& metadata,
                  mojom::PageLoadFeaturesPtr new_features,
                  std::vector<mojom::ResourceDataUpdatePtr> resources,
                  const mojom::FrameRenderDataUpdate& render_data,
                  const mojom::CpuTimingPtr& cpu_timing,
                  mojom::DeferredResourceCountsPtr new_deferred_resource_data,
                  mojom::InputTimingPtr new_input_timing) override;

 private:
  PageTimingValidator* const validator_;
  DISALLOW_COPY_AND_ASSIGN(FakePageTimingSender);
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_FAKE_PAGE_TIMING_SENDER_H_
