// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_load_metrics/browser/observers/declarative_performance_observer.h"

#include "base/test/scoped_feature_list.h"
#include "components/page_load_metrics/browser/observers/page_load_metrics_observer_content_test_harness.h"
#include "content/public/test/mock_navigation_handle.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_load_metrics {

class DeclarativePerformanceObserverTest
    : public content::RenderViewHostTestHarness {
 public:
  DeclarativePerformanceObserverTest() = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    observer_ = std::make_unique<DeclarativePerformanceObserver>();
  }

  DeclarativePerformanceObserver* observer() { return observer_.get(); }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<DeclarativePerformanceObserver> observer_;
};

TEST_F(DeclarativePerformanceObserverTest, OnStartEnabled) {
  feature_list_.InitAndEnableFeature(
      network::features::kDeclarativePerformanceObserver);

  content::MockNavigationHandle handle;

  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer()->OnStart(&handle, GURL("https://example.com"), true));
}

TEST_F(DeclarativePerformanceObserverTest, OnStartDisabled) {
  feature_list_.InitAndDisableFeature(
      network::features::kDeclarativePerformanceObserver);

  content::MockNavigationHandle handle;

  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer()->OnStart(&handle, GURL("https://example.com"), true));
}

TEST_F(DeclarativePerformanceObserverTest, OnCommitWithPolicy) {
  content::MockNavigationHandle handle;

  auto policy = network::mojom::DeclarativePerformanceObserverPolicy::New();
  policy->reporting_endpoint = "default";
  // Add at least one entry type to pass the empty check.
  policy->entry_types.push_back(
      network::mojom::PerformanceEntryType::kNavigation);

  EXPECT_CALL(handle, GetDeclarativePerformanceObserverPolicy())
      .WillRepeatedly(testing::Return(policy.get()));

  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::CONTINUE_OBSERVING,
            observer()->OnCommit(&handle));
}

TEST_F(DeclarativePerformanceObserverTest, OnCommitWithoutPolicy) {
  content::MockNavigationHandle handle;

  EXPECT_CALL(handle, GetDeclarativePerformanceObserverPolicy())
      .WillRepeatedly(testing::Return(nullptr));

  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer()->OnCommit(&handle));
}

TEST_F(DeclarativePerformanceObserverTest, OnCommitWithNullReportingEndpoint) {
  content::MockNavigationHandle handle;

  auto policy = network::mojom::DeclarativePerformanceObserverPolicy::New();
  // Leave reporting_endpoint as null.
  policy->entry_types.push_back(
      network::mojom::PerformanceEntryType::kNavigation);

  EXPECT_CALL(handle, GetDeclarativePerformanceObserverPolicy())
      .WillRepeatedly(testing::Return(policy.get()));

  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer()->OnCommit(&handle));
}

TEST_F(DeclarativePerformanceObserverTest, OnCommitWithEmptyEntryTypes) {
  content::MockNavigationHandle handle;

  auto policy = network::mojom::DeclarativePerformanceObserverPolicy::New();
  policy->reporting_endpoint = "default";
  // Leave entry_types as empty.

  EXPECT_CALL(handle, GetDeclarativePerformanceObserverPolicy())
      .WillRepeatedly(testing::Return(policy.get()));

  EXPECT_EQ(page_load_metrics::PageLoadMetricsObserver::STOP_OBSERVING,
            observer()->OnCommit(&handle));
}

}  // namespace page_load_metrics
