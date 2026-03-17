// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_annotations_common.h"

#include <cmath>

#include "base/test/scoped_feature_list.h"
#include "components/page_content_annotations/core/page_content_annotations_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_content_annotations {

TEST(PageContentAnnotationsCommonTest, GenerateRapporNoisedScore) {
  uint32_t num_buckets = std::pow(2, features::NumBitsForRAPPORMetrics());

  // The noised score returned is the bucket index and therefore will be bounded
  // between 0 and the number of buckets.
  int64_t noised_score_0 = GenerateRapporNoisedScore(0);
  EXPECT_GE(noised_score_0, 0);
  EXPECT_LE(noised_score_0, num_buckets);

  int64_t noised_score_37 = GenerateRapporNoisedScore(0.37);
  EXPECT_GE(noised_score_37, 0);
  EXPECT_LE(noised_score_37, num_buckets);

  int64_t noised_score_100 = GenerateRapporNoisedScore(1.0);
  EXPECT_GE(noised_score_100, 0);
  EXPECT_LE(noised_score_100, num_buckets);
}

TEST(PageContentAnnotationsCommonTest,
     GenerateNoiseScoreAssignsCorrectBuckets) {
  base::test::ScopedFeatureList scoped_feature_list;
  // With noise probability at 0, `GenerateRapporNoisedScore` is purely a
  // bucketing function with deterministic output.
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kPageContentAnnotations,
      {{"num_bits_for_rappor_metrics", "4"},
       {"noise_prob_for_rappor_metrics", "0.0"}});

  // 4 bits gives 16 buckets. Bucket size = 100.0 / 16 = 6.25.
  EXPECT_EQ(0, GenerateRapporNoisedScore(0));
  EXPECT_EQ(0, GenerateRapporNoisedScore(0.06));
  EXPECT_EQ(5, GenerateRapporNoisedScore(0.37));
  EXPECT_EQ(15, GenerateRapporNoisedScore(0.99));
  // Edge case: 100 / 6.25 = 16.0, which should be capped at 15.
  EXPECT_EQ(15, GenerateRapporNoisedScore(1.0));
}

}  // namespace page_content_annotations
