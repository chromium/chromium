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

}  // namespace page_load_metrics
