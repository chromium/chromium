// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/use_counter_page_load_metrics_observer.h"

#include <memory>
#include <vector>

#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "components/page_load_metrics/browser/page_load_tracker.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "url/gurl.h"

namespace {

const char kTestUrl[] = "https://www.google.com";
const char kFencedFramesUrl[] = "https://a.test/fenced_frames";
using WebFeature = blink::mojom::WebFeature;
using CSSSampleId = blink::mojom::CSSSampleId;
using FeatureType = blink::mojom::UseCounterFeatureType;

const char* GetUseCounterHistogramName(
    blink::mojom::UseCounterFeatureType feature_type,
    bool is_in_main_frame = false,
    bool is_in_fenced_frames = false) {
  if (is_in_fenced_frames) {
    if (is_in_main_frame) {
      CHECK_EQ(FeatureType::kWebFeature, feature_type);
      return "Blink.UseCounter.FencedFrames.MainFrame.Features";
    }
    switch (feature_type) {
      case FeatureType::kWebFeature:
        return "Blink.UseCounter.FencedFrames.Features";
      case FeatureType::kCssProperty:
        return "Blink.UseCounter.FencedFrames.CSSProperties";
      case FeatureType::kAnimatedCssProperty:
        return "Blink.UseCounter.FencedFrames.AnimatedCSSProperties";
      case FeatureType::kPermissionsPolicyViolationEnforce:
        return "Blink.UseCounter.FencedFrames.PermissionsPolicy.Violation."
               "Enforce";
      case FeatureType::kPermissionsPolicyHeader:
        return "Blink.UseCounter.FencedFrames.PermissionsPolicy.Header2";
      case FeatureType::kPermissionsPolicyIframeAttribute:
        return "Blink.UseCounter.FencedFrames.PermissionsPolicy.Allow2";
    }
  } else {
    if (is_in_main_frame) {
      CHECK_EQ(FeatureType::kWebFeature, feature_type);
      return "Blink.UseCounter.MainFrame.Features";
    }
    switch (feature_type) {
      case FeatureType::kWebFeature:
        return "Blink.UseCounter.Features";
      case FeatureType::kCssProperty:
        return "Blink.UseCounter.CSSProperties";
      case FeatureType::kAnimatedCssProperty:
        return "Blink.UseCounter.AnimatedCSSProperties";
      case FeatureType::kPermissionsPolicyViolationEnforce:
        return "Blink.UseCounter.PermissionsPolicy.Violation.Enforce";
      case FeatureType::kPermissionsPolicyHeader:
        return "Blink.UseCounter.PermissionsPolicy.Header2";
      case FeatureType::kPermissionsPolicyIframeAttribute:
        return "Blink.UseCounter.PermissionsPolicy.Allow2";
    }
  }
}

}  // namespace

// Note that this test inherits PageLoadMetricsObserverContentTestHarness only
// to get a valid RenderFrameHost*.
class UseCounterMetricsRecorderTest
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 protected:
  bool WithFencedFrames() const { return std::get<0>(GetParam()); }

  bool WithPrerendering() const { return std::get<1>(GetParam()); }

  void ExpectBucketCount(const blink::UseCounterFeature& feature,
                         size_t count) {
    if (feature.type() == blink::mojom::UseCounterFeatureType::kWebFeature) {
      tester()->histogram_tester().ExpectBucketCount(
          GetUseCounterHistogramName(FeatureType::kWebFeature, true,
                                     WithFencedFrames()),
          static_cast<base::Histogram::Sample>(feature.value()), count);
    }

    tester()->histogram_tester().ExpectBucketCount(
        GetUseCounterHistogramName(feature.type(), false, WithFencedFrames()),
        static_cast<base::Histogram::Sample>(feature.value()), count);
  }

  void HistogramBasicTest(
      const std::vector<blink::UseCounterFeature>& first_features,
      const std::vector<blink::UseCounterFeature>& second_features = {}) {
    UseCounterMetricsRecorder recorder(WithFencedFrames());
    ukm::SourceId ukm_source_id = ukm::AssignNewSourceId();

    // Get a valid RenderFrameHost*.
    NavigateAndCommit(GURL(kTestUrl));
    content::RenderFrameHost* rfh = nullptr;
    if (WithFencedFrames()) {
      content::RenderFrameHost* fenced_frame_root =
          content::RenderFrameHostTester::For(
              web_contents()->GetPrimaryMainFrame())
              ->AppendFencedFrame();
      ASSERT_TRUE(fenced_frame_root->IsFencedFrameRoot());

      auto simulator = content::NavigationSimulator::CreateRendererInitiated(
          GURL(kFencedFramesUrl), fenced_frame_root);
      ASSERT_NE(nullptr, simulator);
      simulator->Commit();
      rfh = simulator->GetFinalRenderFrameHost();
    } else {
      rfh = main_rfh();
    }

    // Simulates initial events.
    recorder.RecordOrDeferMainFrameWebFeature(rfh, WebFeature::kPageVisits);
    recorder.RecordOrDeferUseCounterFeature(
        rfh, {blink::mojom::UseCounterFeatureType::kWebFeature,
              static_cast<int>(WebFeature::kPageVisits)});
    recorder.RecordOrDeferUseCounterFeature(
        rfh,
        {FeatureType::kCssProperty,
         static_cast<int>(blink::mojom::CSSSampleId::kTotalPagesMeasured)});
    recorder.RecordOrDeferUseCounterFeature(
        rfh,
        {FeatureType::kAnimatedCssProperty,
         static_cast<int>(blink::mojom::CSSSampleId::kTotalPagesMeasured)});

    // Simulates OnCommit.
    if (!WithPrerendering()) {
      recorder.RecordUkmPageVisits(ukm_source_id);
      recorder.DisableDeferAndFlush();
    }

    // Simulates OnFeaturesUsageObserved.
    for (const auto& feature : first_features) {
      if (feature.type() == FeatureType::kWebFeature) {
        recorder.RecordOrDeferMainFrameWebFeature(
            rfh, static_cast<WebFeature>(feature.value()));
      }
      recorder.RecordOrDeferUseCounterFeature(rfh, feature);
    }

    // Simulates DidActivatePrerenderedPage
    if (WithPrerendering()) {
      // No metrics are recorded in prerendering.
      EXPECT_EQ(0u, tester()
                        ->histogram_tester()
                        .GetTotalCountsForPrefix("Blink.UseCounter.")
                        .size());

      recorder.RecordUkmPageVisits(ukm_source_id);
      recorder.DisableDeferAndFlush();
    }

    // Verify that kPageVisits is observed on commit.
    tester()->histogram_tester().ExpectBucketCount(
        GetUseCounterHistogramName(FeatureType::kWebFeature, false,
                                   WithFencedFrames()),
        static_cast<base::Histogram::Sample>(WebFeature::kPageVisits), 1);
    tester()->histogram_tester().ExpectBucketCount(
        GetUseCounterHistogramName(FeatureType::kWebFeature, true,
                                   WithFencedFrames()),
        static_cast<base::Histogram::Sample>(WebFeature::kPageVisits), 1);

    // Verify that page visit is recorded for CSS histograms.
    tester()->histogram_tester().ExpectBucketCount(
        GetUseCounterHistogramName(FeatureType::kCssProperty, false,
                                   WithFencedFrames()),
        blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
    tester()->histogram_tester().ExpectBucketCount(
        GetUseCounterHistogramName(FeatureType::kAnimatedCssProperty, false,
                                   WithFencedFrames()),
        blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);

    for (const auto& feature : first_features)
      ExpectBucketCount(feature, 1ul);

    // Simulates OnFeaturesUsageObserved.
    for (const auto& feature : second_features) {
      if (feature.type() == FeatureType::kWebFeature) {
        recorder.RecordOrDeferMainFrameWebFeature(
            rfh, static_cast<WebFeature>(feature.value()));
      }
      recorder.RecordOrDeferUseCounterFeature(rfh, feature);
    }

    for (const auto& feature : first_features)
      ExpectBucketCount(feature, 1ul);
    for (const auto& feature : second_features)
      ExpectBucketCount(feature, 1ul);
  }
};

INSTANTIATE_TEST_SUITE_P(All,
                         UseCounterMetricsRecorderTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(UseCounterMetricsRecorderTest, CountOneFeature) {
  HistogramBasicTest({{blink::mojom::UseCounterFeatureType::kWebFeature, 0}});
}

TEST_P(UseCounterMetricsRecorderTest, CountFeatures) {
  HistogramBasicTest(
      {
          {blink::mojom::UseCounterFeatureType::kWebFeature, 0},
          {blink::mojom::UseCounterFeatureType::kWebFeature, 1},
          {blink::mojom::UseCounterFeatureType::kCssProperty, 1},
      },
      {
          {blink::mojom::UseCounterFeatureType::kWebFeature, 2},
          {blink::mojom::UseCounterFeatureType::kAnimatedCssProperty, 2},
          {blink::mojom::UseCounterFeatureType::
               kPermissionsPolicyViolationEnforce,
           3},
      });
}

TEST_P(UseCounterMetricsRecorderTest, CountDuplicatedFeatures) {
  HistogramBasicTest(
      {
          {blink::mojom::UseCounterFeatureType::kWebFeature, 0},
          {blink::mojom::UseCounterFeatureType::kWebFeature, 0},
          {blink::mojom::UseCounterFeatureType::kWebFeature, 1},
          {blink::mojom::UseCounterFeatureType::kCssProperty, 1},
          {blink::mojom::UseCounterFeatureType::kCssProperty, 1},
          {blink::mojom::UseCounterFeatureType::kAnimatedCssProperty, 2},
          {blink::mojom::UseCounterFeatureType::
               kPermissionsPolicyViolationEnforce,
           3},
          {blink::mojom::UseCounterFeatureType::kCssProperty, 3},
      },
      {
          {blink::mojom::UseCounterFeatureType::kWebFeature, 0},
          {blink::mojom::UseCounterFeatureType::kWebFeature, 2},
          {blink::mojom::UseCounterFeatureType::kAnimatedCssProperty, 2},
          {blink::mojom::UseCounterFeatureType::
               kPermissionsPolicyViolationEnforce,
           3},
      });
}

class UseCounterPageLoadMetricsObserverTest
    : public page_load_metrics::PageLoadMetricsObserverContentTestHarness,
      public testing::WithParamInterface<bool> {
 public:
  UseCounterPageLoadMetricsObserverTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kFencedFrames, {{"implementation_type", "mparch"}});
  }

  UseCounterPageLoadMetricsObserverTest(
      const UseCounterPageLoadMetricsObserverTest&) = delete;
  UseCounterPageLoadMetricsObserverTest& operator=(
      const UseCounterPageLoadMetricsObserverTest&) = delete;

  void ExpectBucketCount(const blink::UseCounterFeature& feature,
                         size_t count) {
    if (feature.type() == blink::mojom::UseCounterFeatureType::kWebFeature) {
      tester()->histogram_tester().ExpectBucketCount(
          GetUseCounterHistogramName(FeatureType::kWebFeature, true),
          static_cast<base::Histogram::Sample>(feature.value()), count);
    }

    tester()->histogram_tester().ExpectBucketCount(
        GetUseCounterHistogramName(feature.type()),
        static_cast<base::Histogram::Sample>(feature.value()), count);
  }

  void HistogramBasicTest(
      const std::vector<blink::UseCounterFeature>& first_features,
      const std::vector<blink::UseCounterFeature>& second_features = {}) {
    NavigateAndCommit(GURL(kTestUrl));

    if (WithFencedFrames()) {
      content::RenderFrameHost* fenced_frame_root =
          content::RenderFrameHostTester::For(
              web_contents()->GetPrimaryMainFrame())
              ->AppendFencedFrame();
      ASSERT_TRUE(fenced_frame_root->IsFencedFrameRoot());

      auto simulator = content::NavigationSimulator::CreateRendererInitiated(
          GURL(kFencedFramesUrl), fenced_frame_root);
      ASSERT_NE(nullptr, simulator);
      simulator->Commit();
    }

    tester()->SimulateFeaturesUpdate(first_features);
    // Verify that kPageVisits is observed on commit.
    tester()->histogram_tester().ExpectBucketCount(
        GetUseCounterHistogramName(FeatureType::kWebFeature),
        static_cast<base::Histogram::Sample>(WebFeature::kPageVisits), 1);
    tester()->histogram_tester().ExpectBucketCount(
        GetUseCounterHistogramName(FeatureType::kWebFeature, true),
        static_cast<base::Histogram::Sample>(WebFeature::kPageVisits), 1);
    // Verify that page visit is recorded for CSS histograms.
    tester()->histogram_tester().ExpectBucketCount(
        GetUseCounterHistogramName(FeatureType::kCssProperty),
        blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);
    tester()->histogram_tester().ExpectBucketCount(
        GetUseCounterHistogramName(FeatureType::kAnimatedCssProperty),
        blink::mojom::CSSSampleId::kTotalPagesMeasured, 1);

    for (const auto& feature : first_features)
      ExpectBucketCount(feature, 1ul);

    tester()->SimulateFeaturesUpdate(second_features);

    for (const auto& feature : first_features)
      ExpectBucketCount(feature, 1ul);
    for (const auto& feature : second_features)
      ExpectBucketCount(feature, 1ul);
  }

 protected:
  void RegisterObservers(page_load_metrics::PageLoadTracker* tracker) override {
    tracker->AddObserver(std::make_unique<UseCounterPageLoadMetricsObserver>());
  }

  bool WithFencedFrames() { return GetParam(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         UseCounterPageLoadMetricsObserverTest,
                         testing::Bool());

TEST_P(UseCounterPageLoadMetricsObserverTest, CountOneFeature) {
  HistogramBasicTest({{blink::mojom::UseCounterFeatureType::kWebFeature, 0}});
}

TEST_P(UseCounterPageLoadMetricsObserverTest, CountFeatures) {
  HistogramBasicTest(
      {
          {blink::mojom::UseCounterFeatureType::kWebFeature, 0},
          {blink::mojom::UseCounterFeatureType::kWebFeature, 1},
          {blink::mojom::UseCounterFeatureType::kCssProperty, 1},
      },
      {
          {blink::mojom::UseCounterFeatureType::kWebFeature, 2},
          {blink::mojom::UseCounterFeatureType::kAnimatedCssProperty, 2},
          {blink::mojom::UseCounterFeatureType::
               kPermissionsPolicyViolationEnforce,
           3},
      });
}

TEST_P(UseCounterPageLoadMetricsObserverTest, CountDuplicatedFeatures) {
  HistogramBasicTest(
      {
          {blink::mojom::UseCounterFeatureType::kWebFeature, 0},
          {blink::mojom::UseCounterFeatureType::kWebFeature, 0},
          {blink::mojom::UseCounterFeatureType::kWebFeature, 1},
          {blink::mojom::UseCounterFeatureType::kCssProperty, 1},
          {blink::mojom::UseCounterFeatureType::kCssProperty, 1},
          {blink::mojom::UseCounterFeatureType::kAnimatedCssProperty, 2},
          {blink::mojom::UseCounterFeatureType::
               kPermissionsPolicyViolationEnforce,
           3},
          {blink::mojom::UseCounterFeatureType::kCssProperty, 3},
      },
      {
          {blink::mojom::UseCounterFeatureType::kWebFeature, 0},
          {blink::mojom::UseCounterFeatureType::kWebFeature, 2},
          {blink::mojom::UseCounterFeatureType::kAnimatedCssProperty, 2},
          {blink::mojom::UseCounterFeatureType::
               kPermissionsPolicyViolationEnforce,
           3},
      });
}
