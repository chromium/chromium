// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/reporting/metrics/fake_sampler.h"
#include "components/reporting/metrics/multi_samplers_collector.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace reporting {
namespace {

TEST(MultiSamplersCollectorTest, Default) {
  base::test::TaskEnvironment task_environment;

  test::FakeDelayedSampler sampler1;
  MetricData metric_data1;
  metric_data1.mutable_telemetry_data()->mutable_audio_telemetry();
  sampler1.SetMetricData(std::move(metric_data1));

  test::FakeDelayedSampler sampler2;
  MetricData metric_data2;
  metric_data2.mutable_telemetry_data()->mutable_boot_performance_telemetry();
  sampler2.SetMetricData(std::move(metric_data2));

  base::test::TestFuture<absl::optional<MetricData>> future;
  {
    auto multi_samplers_collector =
        base::MakeRefCounted<MultiSamplersCollector>(future.GetCallback());

    // Collect data from samplers.
    multi_samplers_collector->Collect(&sampler1);
    multi_samplers_collector->Collect(&sampler2);
  }
  sampler2.RunCallback();
  sampler1.RunCallback();

  absl::optional<MetricData> result = future.Take();

  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_telemetry_data());
  EXPECT_TRUE(result->telemetry_data().has_audio_telemetry());
  EXPECT_TRUE(result->telemetry_data().has_boot_performance_telemetry());
}

TEST(MultiSamplersCollectorTest, OneEmptyResult) {
  base::test::TaskEnvironment task_environment;

  test::FakeDelayedSampler sampler1;
  MetricData metric_data1;
  metric_data1.mutable_telemetry_data()->mutable_audio_telemetry();
  sampler1.SetMetricData(std::move(metric_data1));

  test::FakeDelayedSampler sampler2;
  sampler2.SetMetricData(absl::nullopt);

  base::test::TestFuture<absl::optional<MetricData>> future;
  {
    auto multi_samplers_collector =
        base::MakeRefCounted<MultiSamplersCollector>(future.GetCallback());

    // Collect data from samplers.
    multi_samplers_collector->Collect(&sampler1);
    multi_samplers_collector->Collect(&sampler2);
  }
  sampler2.RunCallback();
  sampler1.RunCallback();

  absl::optional<MetricData> result = future.Take();

  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->has_telemetry_data());
  EXPECT_TRUE(result->telemetry_data().has_audio_telemetry());
}

TEST(MultiSamplersCollectorTest, AllResultsEmpty) {
  base::test::TaskEnvironment task_environment;

  test::FakeDelayedSampler sampler1;
  sampler1.SetMetricData(absl::nullopt);

  test::FakeDelayedSampler sampler2;
  sampler2.SetMetricData(absl::nullopt);

  base::test::TestFuture<absl::optional<MetricData>> future;
  {
    auto multi_samplers_collector =
        base::MakeRefCounted<MultiSamplersCollector>(future.GetCallback());

    // Collect data from samplers.
    multi_samplers_collector->Collect(&sampler1);
    multi_samplers_collector->Collect(&sampler2);
  }
  sampler2.RunCallback();
  sampler1.RunCallback();

  absl::optional<MetricData> result = future.Take();

  ASSERT_FALSE(result.has_value());
}

TEST(MultiSamplersCollectorTest, CallbackDropped) {
  base::test::TaskEnvironment task_environment;

  base::test::TestFuture<absl::optional<MetricData>> future;
  {
    test::FakeDelayedSampler sampler;
    MetricData metric_data;
    metric_data.mutable_telemetry_data()->mutable_audio_telemetry();
    sampler.SetMetricData(std::move(metric_data));

    auto multi_samplers_collector =
        base::MakeRefCounted<MultiSamplersCollector>(future.GetCallback());

    // Collect data from samplers.
    multi_samplers_collector->Collect(&sampler);
    // `sampler` goes out of scope without running the callback.
  }
  absl::optional<MetricData> result = future.Take();

  ASSERT_FALSE(result.has_value());
}

}  // namespace
}  // namespace reporting
