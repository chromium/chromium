// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PAGE_LOAD_METRICS_FAKE_PAGE_TIMING_SENDER_H_
#define CHROME_RENDERER_PAGE_LOAD_METRICS_FAKE_PAGE_TIMING_SENDER_H_

#include <set>
#include <vector>

#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"
#include "chrome/common/page_load_metrics/page_load_timing.h"
#include "chrome/renderer/page_load_metrics/page_timing_sender.h"

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

    // Forces verification that actual timings sent through SendTiming() match
    // expected timings provided via ExpectPageLoadTiming.
    void VerifyExpectedTimings() const;

    // PageLoad features that are expected to be sent through SendTiming()
    // should be passed via UpdateExpectedPageLoadFeatures.
    void UpdateExpectPageLoadFeatures(const blink::mojom::WebFeature feature);
    // PageLoad CSS properties that are expected to be sent through SendTiming()
    // should be passed via UpdateExpectedPageLoadCSSProperties.
    void UpdateExpectPageLoadCssProperties(int css_property_id);

    void UpdateExpectPageRenderData(const mojom::PageRenderData& render_data) {
      expected_render_data_ = render_data;
    }

    // Forces verification that actual features sent through SendTiming match
    // expected features provided via ExpectPageLoadFeatures.
    void VerifyExpectedFeatures() const;
    // Forces verification that actual CSS properties sent through SendTiming
    // match expected CSS properties provided via ExpectPageLoadCSSProperties.
    void VerifyExpectedCssProperties() const;
    void VerifyExpectedRenderData() const;

    const std::vector<mojom::PageLoadTimingPtr>& expected_timings() const {
      return expected_timings_;
    }
    const std::vector<mojom::PageLoadTimingPtr>& actual_timings() const {
      return actual_timings_;
    }

    void UpdateTiming(
        const mojom::PageLoadTimingPtr& timing,
        const mojom::PageLoadMetadataPtr& metadata,
        const mojom::PageLoadFeaturesPtr& new_features,
        const std::vector<mojom::ResourceDataUpdatePtr>& resources,
        const mojom::PageRenderData& render_data);

   private:
    std::vector<mojom::PageLoadTimingPtr> expected_timings_;
    std::vector<mojom::PageLoadTimingPtr> actual_timings_;
    std::set<blink::mojom::WebFeature> expected_features_;
    std::set<blink::mojom::WebFeature> actual_features_;
    std::set<int> expected_css_properties_;
    std::set<int> actual_css_properties_;
    mojom::PageRenderData expected_render_data_;
    mojom::PageRenderData actual_render_data_;
    DISALLOW_COPY_AND_ASSIGN(PageTimingValidator);
  };

  explicit FakePageTimingSender(PageTimingValidator* validator);
  ~FakePageTimingSender() override;
  void SendTiming(const mojom::PageLoadTimingPtr& timing,
                  const mojom::PageLoadMetadataPtr& metadata,
                  mojom::PageLoadFeaturesPtr new_features,
                  std::vector<mojom::ResourceDataUpdatePtr> resources,
                  const mojom::PageRenderData& render_data) override;

 private:
  PageTimingValidator* const validator_;
  DISALLOW_COPY_AND_ASSIGN(FakePageTimingSender);
};

}  // namespace page_load_metrics

#endif  // CHROME_RENDERER_PAGE_LOAD_METRICS_FAKE_PAGE_TIMING_SENDER_H_
