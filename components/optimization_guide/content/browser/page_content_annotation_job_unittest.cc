// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/content/browser/page_content_annotation_job.h"

#include "base/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class PageContentAnnotationJobTest : public testing::Test {};

TEST_F(PageContentAnnotationJobTest, State) {
  PageContentAnnotationJob job("input", AnnotationType::kPageTopics);
  EXPECT_EQ(job.input(), "input");
  EXPECT_EQ(job.type(), AnnotationType::kPageTopics);
  EXPECT_EQ(job.status(), ExecutionStatus::kUnknown);
  EXPECT_EQ(job.page_topics(), absl::nullopt);
  EXPECT_EQ(job.page_entities(), absl::nullopt);
  EXPECT_EQ(job.visibility_score(), absl::nullopt);

  job.SetStatus(ExecutionStatus::kPending);
  EXPECT_EQ(job.status(), ExecutionStatus::kPending);
}

TEST_F(PageContentAnnotationJobTest, FinalizePageTopics) {
  PageContentAnnotationJob job("input", AnnotationType::kPageTopics);
  job.SetPageTopicsOutput(std::vector<WeightedString>{});

  EXPECT_NE(job.page_topics(), absl::nullopt);
  EXPECT_EQ(job.page_entities(), absl::nullopt);
  EXPECT_EQ(job.visibility_score(), absl::nullopt);
}

TEST_F(PageContentAnnotationJobTest, FinalizePageEntites) {
  PageContentAnnotationJob job("input", AnnotationType::kPageEntities);
  job.SetPageEntitiesOutput(std::vector<WeightedString>{});

  EXPECT_EQ(job.page_topics(), absl::nullopt);
  EXPECT_NE(job.page_entities(), absl::nullopt);
  EXPECT_EQ(job.visibility_score(), absl::nullopt);
}

TEST_F(PageContentAnnotationJobTest, FinalizeContentVisibility) {
  PageContentAnnotationJob job("input", AnnotationType::kContentVisibility);
  job.SetVisibilityScoreOutput(1.0);

  EXPECT_EQ(job.page_topics(), absl::nullopt);
  EXPECT_EQ(job.page_entities(), absl::nullopt);
  EXPECT_NE(job.visibility_score(), absl::nullopt);
}

}  // namespace optimization_guide