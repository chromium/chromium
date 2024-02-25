// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/metrics/delayed_sampler.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace reporting {

namespace {

class DelayedSamplerTest : public ::testing::Test {
 protected:
  void SetUp() override { sampler_ = std::make_unique<test::FakeSampler>(); }
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<test::FakeSampler> sampler_;
};

TEST_F(DelayedSamplerTest, CollectInternalCalledAfterDelay) {
  base::TimeDelta delay = base::Seconds(1);

  test::FakeSampler* raw_sampler_ = sampler_.get();

  DelayedSampler delayed_sampler(std::move(sampler_), delay);

  // Haven't called `MaybeCollect`, so we definitely should not have tried to
  // collect data yet.
  EXPECT_THAT(raw_sampler_->GetNumCollectCalls(), Eq(0));

  delayed_sampler.MaybeCollect(
      base::DoNothingAs<void(std::optional<MetricData>)>());

  // Sampler shouldn't collect since no time has passed
  EXPECT_THAT(raw_sampler_->GetNumCollectCalls(), Eq(0));

  task_environment_.FastForwardBy(delay);

  // `delay` amount of time has passed since `MaybeCollect` was called, so the
  // sampler should have collected now.
  EXPECT_THAT(raw_sampler_->GetNumCollectCalls(), Eq(1));
}
}  // namespace
}  // namespace reporting
