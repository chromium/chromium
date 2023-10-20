// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/performance_manager/resource_attribution/query_scheduler.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/performance_manager/embedder/graph_features.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/scoped_cpu_query.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/resource_attribution/simulated_cpu_measurement_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::resource_attribution {

namespace {

using ::testing::Contains;
using ::testing::Key;

}  // namespace

class QuerySchedulerTest : public GraphTestHarness {
 protected:
  using Super = GraphTestHarness;

  void SetUp() override {
    GetGraphFeatures().EnableResourceAttributionScheduler();
    Super::SetUp();
  }

  // This must be deleted after TearDown() so that it outlives the
  // CPUMeasurementMonitor.
  SimulatedCPUMeasurementDelegateFactory delegate_factory_;
};

TEST_F(QuerySchedulerTest, CPUQueries) {
  MockSinglePageInSingleProcessGraph mock_graph(graph());
  CPUMeasurementDelegate::SetDelegateFactoryForTesting(
      graph(), delegate_factory_.GetFactoryCallback());
  delegate_factory_.SetDefaultCPUUsage(99);

  auto* scheduler = QueryScheduler::GetFromGraph(graph());
  ASSERT_TRUE(scheduler);

  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  auto cpu_query1 = std::make_unique<ScopedCPUQuery>(graph());

  // First query created should start CPU monitoring.
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());

  // Allow some time to pass to measure.
  task_env().FastForwardBy(base::Minutes(1));
  EXPECT_THAT(cpu_query1->QueryOnce(),
              Contains(Key(mock_graph.process->resource_context())));

  // CPU monitoring should not stop until the last query is deleted.
  auto cpu_query2 = std::make_unique<ScopedCPUQuery>(graph());
  cpu_query1.reset();
  EXPECT_TRUE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
  EXPECT_THAT(cpu_query2->QueryOnce(),
              Contains(Key(mock_graph.process->resource_context())));

  cpu_query2.reset();
  EXPECT_FALSE(scheduler->GetCPUMonitorForTesting().IsMonitoring());
}

}  // namespace performance_manager::resource_attribution
