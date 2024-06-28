// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAGE_LOAD_METRICS_RENDERER_FAKE_PAGE_TIMING_SENDER_H_
#define COMPONENTS_PAGE_LOAD_METRICS_RENDERER_FAKE_PAGE_TIMING_SENDER_H_

#include <set>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/page_load_metrics/common/page_load_metrics.mojom.h"
#include "components/page_load_metrics/common/page_load_timing.h"
#include "components/page_load_metrics/renderer/page_timing_sender.h"
#include "third_party/blink/public/common/use_counter/use_counter_feature.h"

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
// std::optional which uses aligned memory, so we're forced to roll
// our own implementation here. See
// https://groups.google.com/forum/#!topic/googletestframework/W-Hud3j_c6I for
// more details.
class FakePageTimingSender : public PageTimingSender {
 public:
  class PageTimingValidator {
   public:
    PageTimingValidator();

    PageTimingValidator(const PageTimingValidator&) = delete;
    PageTimingValidator& operator=(const PageTimingValidator&) = delete;

    ~PageTimingValidator();
    // PageLoadTimings that are expected to be sent through SendTiming() should
    // be passed to ExpectPageLoadTiming.
    void ExpectPageLoadTiming(const mojom::PageLoadTiming& timing);

    void ExpectSoftNavigationMetrics(
        const mojom::SoftNavigationMetrics& soft_navigation_metrics);
    // CpuTimings that are expected to be sent through SendTiming() should be
    // passed to ExpectCpuTiming.
    void ExpectCpuTiming(const base::TimeDelta& timing);

    // Forces verification that actual timings sent through SendTiming() match
    // expected timings provided via ExpectPageLoadTiming.
    void VerifyExpectedTimings() const;

    void VerifyExpectedSoftNavigationMetrics() const;

    // Forces verification that actual timings sent through SendTiming() match
    // expected timings provided via ExpectCpuTiming.
    void VerifyExpectedCpuTimings() const;

    void VerifyExpectedInteractionTiming() const;

    void VerifyExpectedSubresourceLoadMetrics() const;

    // PageLoad features that are expected to be sent through SendTiming()
    // should be passed via UpdateExpectedPageLoadFeatures.
    void UpdateExpectPageLoadFeatures(const blink::UseCounterFeature& feature);

    void UpdateExpectFrameRenderDataUpdate(
        const mojom::FrameRenderDataUpdate& render_data) {
      expected_render_data_ = render_data.Clone();
    }

    void UpdateExpectedInteractionTiming(
        const base::TimeDelta interaction_duration,
        mojom::UserInteractionType interaction_type,
        uint64_t interaction_offset,
        const base::TimeTicks interaction_time);

    void UpdateExpectedSubresourceLoadMetrics(
        const blink::SubresourceLoadMetrics& subresource_load_metrics);

    void UpdateExpectedMainFrameIntersectionRect(
        const gfx::Rect& main_frame_intersection_rect) {
      expected_main_frame_intersection_rect_ = main_frame_intersection_rect;
    }

    void UpdateExpectedMainFrameViewportRect(
        const gfx::Rect& main_frame_viewport_rect) {
      expected_main_frame_viewport_rect_ = main_frame_viewport_rect;
    }

    // Forces verification that actual features sent through SendTiming match
    // expected features provided via ExpectPageLoadFeatures.
    void VerifyExpectedFeatures() const;
    void VerifyExpectedRenderData() const;
    void VerifyExpectedMainFrameIntersectionRect() const;
    void VerifyExpectedMainFrameViewportRect() const;

    const std::vector<mojom::PageLoadTimingPtr>& expected_timings() const {
      return expected_timings_;
    }
    const std::vector<mojom::PageLoadTimingPtr>& actual_timings() const {
      return actual_timings_;
    }

    void UpdateTiming(
        const mojom::PageLoadTimingPtr& timing,
        const mojom::FrameMetadataPtr& metadata,
        const std::vector<blink::UseCounterFeature>& new_features,
        const std::vector<mojom::ResourceDataUpdatePtr>& resources,
        const mojom::FrameRenderDataUpdate& render_data,
        const mojom::CpuTimingPtr& cpu_timing,
        const mojom::InputTimingPtr& input_timing,
        const std::optional<blink::SubresourceLoadMetrics>&
            subresource_load_metrics,
        const mojom::SoftNavigationMetricsPtr& soft_navigation_metrics);

   private:
    std::vector<mojom::PageLoadTimingPtr> expected_timings_;
    std::vector<mojom::PageLoadTimingPtr> actual_timings_;
    std::vector<mojom::SoftNavigationMetricsPtr>
        expected_soft_navigation_metrics_;
    std::vector<mojom::SoftNavigationMetricsPtr>
        actual_soft_navigation_metrics_;
    std::vector<mojom::CpuTimingPtr> expected_cpu_timings_;
    std::vector<mojom::CpuTimingPtr> actual_cpu_timings_;
    std::set<blink::UseCounterFeature> expected_features_;
    std::set<blink::UseCounterFeature> actual_features_;
    mojom::FrameRenderDataUpdatePtr expected_render_data_;
    mojom::FrameRenderDataUpdate actual_render_data_;
    std::optional<gfx::Rect> expected_main_frame_intersection_rect_;
    std::optional<gfx::Rect> actual_main_frame_intersection_rect_;
    std::optional<gfx::Rect> expected_main_frame_viewport_rect_;
    std::optional<gfx::Rect> actual_main_frame_viewport_rect_;
    mojom::InputTiming expected_input_timing;
    mojom::InputTiming actual_input_timing;
    std::optional<blink::SubresourceLoadMetrics>
        expected_subresource_load_metrics_;
    std::optional<blink::SubresourceLoadMetrics>
        actual_subresource_load_metrics_;
  };

  explicit FakePageTimingSender(PageTimingValidator* validator);

  FakePageTimingSender(const FakePageTimingSender&) = delete;
  FakePageTimingSender& operator=(const FakePageTimingSender&) = delete;

  ~FakePageTimingSender() override;

  void SendTiming(
      const mojom::PageLoadTimingPtr& timing,
      const mojom::FrameMetadataPtr& metadata,
      const std::vector<blink::UseCounterFeature>& new_features,
      std::vector<mojom::ResourceDataUpdatePtr> resources,
      const mojom::FrameRenderDataUpdate& render_data,
      const mojom::CpuTimingPtr& cpu_timing,
      mojom::InputTimingPtr new_input_timing,
      const std::optional<blink::SubresourceLoadMetrics>&
          subresource_load_metrics,
      const mojom::SoftNavigationMetricsPtr& soft_navigation_metrics) override;

  void SetUpSmoothnessReporting(
      base::ReadOnlySharedMemoryRegion shared_memory) override;

  void SendCustomUserTiming(mojom::CustomUserTimingMarkPtr timing) override;

 private:
  const raw_ptr<PageTimingValidator> validator_;
};

}  // namespace page_load_metrics

#endif  // COMPONENTS_PAGE_LOAD_METRICS_RENDERER_FAKE_PAGE_TIMING_SENDER_H_
