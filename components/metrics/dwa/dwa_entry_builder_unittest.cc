// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/dwa/dwa_entry_builder.h"

#include <string>
#include <unordered_set>

#include "base/metrics/metrics_hashes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dwa {

// Tests that calling `SetMetric` repeatedly on an `DwaEntryBuilder` updates the
// value stored for the metric.
TEST(DwaEntryBuilderTest, BuilderAllowsUpdatingMetrics) {
  DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetMetric("Length", 4);
  EXPECT_THAT(
      (*builder.GetEntryForTesting())->metrics,
      testing::ElementsAre(testing::Pair(base::HashMetricName("Length"), 4)));

  builder.SetMetric("Length", 5);
  EXPECT_THAT(
      (*builder.GetEntryForTesting())->metrics,
      testing::ElementsAre(testing::Pair(base::HashMetricName("Length"), 5)));
}

// Tests that calling `AddStudy` repeatedly on an `DwaEntryBuilder` updates the
// value stored for the study.
TEST(DwaEntryBuilderTest, BuilderAllowsAddingStudiesOfInterest) {
  DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.AddToStudiesOfInterest("Study1");
  EXPECT_THAT((*builder.GetEntryForTesting())->studies_of_interest,
              testing::ElementsAre(testing::Pair("Study1", true)));

  builder.AddToStudiesOfInterest("Study2");
  EXPECT_THAT((*builder.GetEntryForTesting())->studies_of_interest,
              testing::ElementsAre(testing::Pair("Study1", true),
                                   testing::Pair("Study2", true)));

  builder.AddToStudiesOfInterest("Study1");
  EXPECT_THAT((*builder.GetEntryForTesting())->studies_of_interest,
              testing::ElementsAre(testing::Pair("Study1", true),
                                   testing::Pair("Study2", true)));
}

TEST(DwaEntryBuilderTest, BuilderAllowsSettingContent) {
  DwaEntryBuilder builder("Kangaroo.Jumped");
  builder.SetContent("content_");
  EXPECT_THAT((*builder.GetEntryForTesting())->content_hash,
              testing::Eq(base::HashMetricName("content_")));

  builder.SetContent("content_");
  EXPECT_THAT((*builder.GetEntryForTesting())->content_hash,
              testing::Eq(base::HashMetricName("content_")));
}

}  // namespace dwa
