// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/memory_pressure/multi_source_memory_pressure_monitor.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace memory_pressure {

TEST(MultiSourceMemoryPressureMonitorTest, NoEvaluatorUponConstruction) {
  MultiSourceMemoryPressureMonitor monitor;
  EXPECT_FALSE(monitor.system_evaluator_for_testing());
}

TEST(MultiSourceMemoryPressureMonitorTest, RunDispatchCallback) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  MultiSourceMemoryPressureMonitor monitor;
  bool callback_called = false;
  monitor.SetDispatchCallbackForTesting(base::BindLambdaForTesting(
      [&](base::MemoryPressureListener::MemoryPressureLevel) {
        callback_called = true;
      }));
  monitor.MaybeStartPlatformVoter();
  auto* const aggregator = monitor.aggregator_for_testing();

  aggregator->OnVoteForTesting(
      absl::nullopt, base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE);
  aggregator->NotifyListenersForTesting();
  EXPECT_TRUE(callback_called);

  // Clear vote so aggregator's destructor doesn't think there are loose voters.
  aggregator->OnVoteForTesting(
      base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE, absl::nullopt);
}

}  // namespace memory_pressure
