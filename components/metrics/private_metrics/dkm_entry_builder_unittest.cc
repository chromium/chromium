// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/private_metrics/dkm_entry_builder.h"

#include "base/metrics/metrics_hashes.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics::private_metrics {
namespace {

// Tests that calling SetMetric() repeatedly on a DkmEntryBuilder updates the
// value stored for the metric.
TEST(DkmEntryBuilderTest, BuilderAllowsUpdatingMetrics) {
  DkmEntryBuilder builder(ukm::kInvalidSourceIdObj, "Kangaroo.Jumped");
  builder.SetMetric("Length", 4);
  EXPECT_THAT(
      (*builder.GetEntryForTesting())->metrics,
      testing::ElementsAre(testing::Pair(base::HashMetricName("Length"), 4)));

  builder.SetMetric("Length", 5);
  EXPECT_THAT(
      (*builder.GetEntryForTesting())->metrics,
      testing::ElementsAre(testing::Pair(base::HashMetricName("Length"), 5)));
}

// Tests that calling AddStudy() repeatedly on a DkmEntryBuilder updates the
// value stored for the study.
TEST(DkmEntryBuilderTest, BuilderAllowsAddingStudiesOfInterest) {
  DkmEntryBuilder builder(ukm::kInvalidSourceIdObj, "Kangaroo.Jumped");
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

}  // namespace
}  // namespace metrics::private_metrics
