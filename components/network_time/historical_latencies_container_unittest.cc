// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/network_time/historical_latencies_container.h"

#include "base/metrics/field_trial_params.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/network_time/network_time_tracker.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network_time {

TEST(HistoricalLatenciesContainerTest, StdDeviation) {
  network_time::HistoricalLatenciesContainer latencies;
  latencies.Record(base::Seconds(1));
  EXPECT_EQ(absl::nullopt, latencies.StdDeviation());

  latencies.Record(base::Seconds(1));
  EXPECT_EQ(absl::nullopt, latencies.StdDeviation());

  latencies.Record(base::Seconds(1));
  EXPECT_THAT(latencies.StdDeviation(), testing::Optional(base::Seconds(0)));

  latencies.Record(base::Seconds(2));
  EXPECT_NE(absl::nullopt, latencies.StdDeviation());
  // The standard deviation of [1,1,2] is 0.816.
  EXPECT_EQ(816, latencies.StdDeviation().value().InMilliseconds());

  latencies.Record(-base::Seconds(10));
  EXPECT_NE(absl::nullopt, latencies.StdDeviation());
  // The standard deviation of [1,2,-10] is 9.416.
  EXPECT_EQ(9416, latencies.StdDeviation().value().InMilliseconds());

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kNetworkTimeServiceQuerying,
        base::FieldTrialParams({{"NumHistoricalLatencies", "2"}}));
    EXPECT_NE(absl::nullopt, latencies.StdDeviation());
    // The standard deviation of [2,-10] is 8.485.
    EXPECT_EQ(8485, latencies.StdDeviation().value().InMilliseconds());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kNetworkTimeServiceQuerying,
        base::FieldTrialParams({{"NumHistoricalLatencies", "0"}}));
    EXPECT_EQ(absl::nullopt, latencies.StdDeviation());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kNetworkTimeServiceQuerying,
        base::FieldTrialParams({{"NumHistoricalLatencies", "-10"}}));
    EXPECT_EQ(absl::nullopt, latencies.StdDeviation());
  }

  {
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeatureWithParameters(
        kNetworkTimeServiceQuerying,
        base::FieldTrialParams({{"NumHistoricalLatencies", "200"}}));
    EXPECT_EQ(absl::nullopt, latencies.StdDeviation());
  }
}

}  // namespace network_time
