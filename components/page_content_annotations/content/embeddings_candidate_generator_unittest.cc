// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/embeddings_candidate_generator.h"

#include <string>
#include <vector>

#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_content_annotations {

namespace {

using testing::ElementsAre;

TEST(EmbeddingsCandidateGeneratorTest, GenerateEmbeddingsCandidates) {
  optimization_guide::proto::AnnotatedPageContent apc;
  apc.mutable_main_frame_data()->set_title("Page Title");
  apc.mutable_main_frame_data()->set_url("https://example.com/");

  auto candidates = GenerateEmbeddingsCandidates(apc, 0);

  // We expect kTitle and kTitleAndUrl candidates.
  // Content candidates are 0 because we passed 0 as the limit.
  ASSERT_EQ(candidates.size(), 2u);

  EXPECT_EQ(candidates[0].first, "Page Title");
  EXPECT_EQ(candidates[0].second, EmbeddingPassageType::kTitle);

  EXPECT_EQ(candidates[1].first, "Page Title - https://example.com/");
  EXPECT_EQ(candidates[1].second, EmbeddingPassageType::kTitleAndUrl);
}

}  // namespace

}  // namespace page_content_annotations
