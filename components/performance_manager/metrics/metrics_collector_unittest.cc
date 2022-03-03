// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/public/metrics/metrics_collector.h"

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/ukm/test_ukm_recorder.h"
#include "url/gurl.h"

namespace performance_manager {

const base::TimeDelta kTestMetricsReportDelayTimeout =
    kMetricsReportDelayTimeout + base::Seconds(1);
const char kHtmlMimeType[] = "text/html";

// TODO(crbug.com/759905) Enable on Windows once this bug is fixed.
#if BUILDFLAG(IS_WIN)
#define MAYBE_MetricsCollectorTest DISABLED_MetricsCollectorTest
#else
#define MAYBE_MetricsCollectorTest MetricsCollectorTest
#endif
class MAYBE_MetricsCollectorTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  MAYBE_MetricsCollectorTest() : GraphTestHarness() {}

  MAYBE_MetricsCollectorTest(const MAYBE_MetricsCollectorTest&) = delete;
  MAYBE_MetricsCollectorTest& operator=(const MAYBE_MetricsCollectorTest&) =
      delete;

  void SetUp() override {
    Super::SetUp();
    auto metrics_collector = std::make_unique<MetricsCollector>();
    metrics_collector_ = metrics_collector.get();
    graph()->PassToGraph(std::move(metrics_collector));
  }

  void TearDown() override {
    graph()->TakeFromGraph(metrics_collector_);  // Destroy the observer.
    metrics_collector_ = nullptr;
    Super::TearDown();
  }

 protected:
  static constexpr uint64_t kDummyID = 1u;

  base::HistogramTester histogram_tester_;

 private:
  raw_ptr<MetricsCollector> metrics_collector_ = nullptr;
};

TEST_F(MAYBE_MetricsCollectorTest, FromBackgroundedToFirstTitleUpdatedUMA) {
  auto page_node = CreateNode<PageNodeImpl>();

  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example.org"),
      kHtmlMimeType);
  AdvanceClock(kTestMetricsReportDelayTimeout);

  page_node->SetIsVisible(true);
  page_node->OnTitleUpdated();
  // The page is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     0);

  page_node->SetIsVisible(false);
  page_node->OnTitleUpdated();
  // The page is backgrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     1);
  page_node->OnTitleUpdated();
  // Metrics should only be recorded once per background period, thus metrics
  // not recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     1);

  page_node->SetIsVisible(true);
  page_node->SetIsVisible(false);
  page_node->OnTitleUpdated();
  // The page is backgrounded from foregrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     2);
}

TEST_F(MAYBE_MetricsCollectorTest,
       FromBackgroundedToFirstTitleUpdatedUMA5MinutesTimeout) {
  auto page_node = CreateNode<PageNodeImpl>();

  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example.org"),
      kHtmlMimeType);
  page_node->SetIsVisible(false);
  page_node->OnTitleUpdated();
  // The page is within 5 minutes after main frame navigation was committed,
  // thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     0);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  page_node->OnTitleUpdated();
  histogram_tester_.ExpectTotalCount(kTabFromBackgroundedToFirstTitleUpdatedUMA,
                                     1);
}

TEST_F(MAYBE_MetricsCollectorTest,
       FromBackgroundedToFirstNonPersistentNotificationCreatedUMA) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process_node.get(), page_node.get());

  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example.org"),
      kHtmlMimeType);
  AdvanceClock(kTestMetricsReportDelayTimeout);

  page_node->SetIsVisible(true);
  frame_node->OnNonPersistentNotificationCreated();
  // The page is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 0);

  page_node->SetIsVisible(false);
  frame_node->OnNonPersistentNotificationCreated();
  // The page is backgrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 1);
  frame_node->OnNonPersistentNotificationCreated();
  // Metrics should only be recorded once per background period, thus metrics
  // not recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 1);

  page_node->SetIsVisible(true);
  page_node->SetIsVisible(false);
  frame_node->OnNonPersistentNotificationCreated();
  // The page is backgrounded from foregrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 2);
}

TEST_F(
    MAYBE_MetricsCollectorTest,
    FromBackgroundedToFirstNonPersistentNotificationCreatedUMA5MinutesTimeout) {
  auto process_node = CreateNode<ProcessNodeImpl>();
  auto page_node = CreateNode<PageNodeImpl>();
  auto frame_node = CreateFrameNodeAutoId(process_node.get(), page_node.get());

  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example.org"),
      kHtmlMimeType);
  page_node->SetIsVisible(false);
  frame_node->OnNonPersistentNotificationCreated();
  // The page is within 5 minutes after main frame navigation was committed,
  // thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 0);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  frame_node->OnNonPersistentNotificationCreated();
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstNonPersistentNotificationCreatedUMA, 1);
}

TEST_F(MAYBE_MetricsCollectorTest, FromBackgroundedToFirstFaviconUpdatedUMA) {
  auto page_node = CreateNode<PageNodeImpl>();

  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example.org"),
      kHtmlMimeType);
  AdvanceClock(kTestMetricsReportDelayTimeout);

  page_node->SetIsVisible(true);
  page_node->OnFaviconUpdated();
  // The page is not backgrounded, thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 0);

  page_node->SetIsVisible(false);
  page_node->OnFaviconUpdated();
  // The page is backgrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 1);
  page_node->OnFaviconUpdated();
  // Metrics should only be recorded once per background period, thus metrics
  // not recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 1);

  page_node->SetIsVisible(true);
  page_node->SetIsVisible(false);
  page_node->OnFaviconUpdated();
  // The page is backgrounded from foregrounded, thus metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 2);
}

TEST_F(MAYBE_MetricsCollectorTest,
       FromBackgroundedToFirstFaviconUpdatedUMA5MinutesTimeout) {
  auto page_node = CreateNode<PageNodeImpl>();

  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), kDummyID, GURL("http://www.example.org"),
      kHtmlMimeType);
  page_node->SetIsVisible(false);
  page_node->OnFaviconUpdated();
  // The page is within 5 minutes after main frame navigation was committed,
  // thus no metrics recorded.
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 0);
  AdvanceClock(kTestMetricsReportDelayTimeout);
  page_node->OnFaviconUpdated();
  histogram_tester_.ExpectTotalCount(
      kTabFromBackgroundedToFirstFaviconUpdatedUMA, 1);
}

}  // namespace performance_manager
