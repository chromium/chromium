// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/embeddings_candidate_generator.h"

#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/page_content_annotations/core/page_embeddings_common.h"
#include "components/passage_embeddings/core/passage_embeddings_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace page_content_annotations {

namespace {

using base::test::WithFeatureOverride;
using testing::ElementsAre;
using testing::Test;

class EmbeddingsCandidateGeneratorTest : public WithFeatureOverride,
                                         public Test {
 public:
  EmbeddingsCandidateGeneratorTest()
      : base::test::WithFeatureOverride(
            passage_embeddings::kPDFEmbeddingsGeneration) {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        passage_embeddings::kPassageEmbedder,
        {{"MaxWordsPerAggregatePassage", "5"}, {"MinWordsPerPassage", "3"}});
  }
  ~EmbeddingsCandidateGeneratorTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_P(EmbeddingsCandidateGeneratorTest, GenerateEmbeddingsCandidatesForAPC) {
  optimization_guide::proto::AnnotatedPageContent apc;
  apc.mutable_main_frame_data()->set_title("Page Title");
  apc.mutable_main_frame_data()->set_url("https://example.com/");

  auto candidates = GenerateEmbeddingsCandidates(
      base::MakeRefCounted<RefCountedAnnotatedPageContent>(apc),
      /*page_content_passages_to_generate=*/0u);

  // We expect kTitle and kTitleAndUrl candidates.
  // Content candidates are 0 because we passed 0 as the limit.
  ASSERT_EQ(candidates.size(), 2u);

  EXPECT_EQ(candidates[0].first, "Page Title");
  EXPECT_EQ(candidates[0].second, EmbeddingPassageType::kTitle);

  EXPECT_EQ(candidates[1].first, "Page Title - https://example.com/");
  EXPECT_EQ(candidates[1].second, EmbeddingPassageType::kTitleAndUrl);
}

TEST_P(EmbeddingsCandidateGeneratorTest,
       GenerateEmbeddingsCandidatesForAPCWithContent) {
  optimization_guide::proto::AnnotatedPageContent apc;
  apc.mutable_main_frame_data()->set_title("Page Title");
  apc.mutable_main_frame_data()->set_url("https://example.com/");

  // Add a text node with text content.
  auto* root_node = apc.mutable_root_node();
  auto* child_0 = root_node->add_children_nodes();
  auto* attr_0 = child_0->mutable_content_attributes();
  attr_0->set_attribute_type(
      optimization_guide::proto::ContentAttributeType::CONTENT_ATTRIBUTE_TEXT);
  attr_0->mutable_text_data()->set_text_content("This is the first paragraph.");

  // Add another text node with text content whose number of word >
  // `MaxWordsPerAggregatePassage`.
  auto* child_1 = root_node->add_children_nodes();
  auto* attr_1 = child_1->mutable_content_attributes();
  attr_1->set_attribute_type(
      optimization_guide::proto::ContentAttributeType::CONTENT_ATTRIBUTE_TEXT);
  attr_1->mutable_text_data()->set_text_content(
      "This paragraph has too many words so it should be dropped from the "
      "final passages.");

  // Add another text node with text content whose number of word <
  // `MinWordsPerPassage`.
  auto* child_2 = root_node->add_children_nodes();
  auto* attr_2 = child_2->mutable_content_attributes();
  attr_2->set_attribute_type(
      optimization_guide::proto::ContentAttributeType::CONTENT_ATTRIBUTE_TEXT);
  attr_2->mutable_text_data()->set_text_content("The end.");

  auto candidates = GenerateEmbeddingsCandidates(
      base::MakeRefCounted<RefCountedAnnotatedPageContent>(apc),
      /*page_content_passages_to_generate=*/10u);

  ASSERT_EQ(candidates.size(), 5u);

  EXPECT_EQ(candidates[0].first, "This is the first paragraph.");
  EXPECT_EQ(candidates[0].second, EmbeddingPassageType::kPageContent);

  // TODO(b/510424894): There is a bug in embeddings candidates generation for
  // APC that `MinWordsPerPassage` and `MaxWordsPerAggregatePassage` are not
  // enforced correctly. Once fixed, exclude the two page-content candidates
  // here; their word counts fall outside the allowed range.
  EXPECT_EQ(candidates[1].first,
            "This paragraph has too many words so it should be dropped from "
            "the final passages.");
  EXPECT_EQ(candidates[1].second, EmbeddingPassageType::kPageContent);
  EXPECT_EQ(candidates[2].first, "The end.");
  EXPECT_EQ(candidates[2].second, EmbeddingPassageType::kPageContent);

  EXPECT_EQ(candidates[3].first, "Page Title");
  EXPECT_EQ(candidates[3].second, EmbeddingPassageType::kTitle);

  EXPECT_EQ(candidates[4].first, "Page Title - https://example.com/");
  EXPECT_EQ(candidates[4].second, EmbeddingPassageType::kTitleAndUrl);
}

// Test embeddings generation for PDF text.
TEST_P(EmbeddingsCandidateGeneratorTest, GenerateEmbeddingsCandidatesForPDF) {
  std::string pdf_text{
      "\r\n \n   This\v  is \tthe   first   paragraph.\n \n\n  \tThis\v     "
      "\tis the \f\n  second    \fparagraph.  \r \r\n\f   \tThe     end. \n\n"};

  auto candidates = GenerateEmbeddingsCandidates(
      base::MakeRefCounted<RefCountedPDFText>(pdf_text),
      /*page_content_passages_to_generate=*/10u);

  if (IsParamFeatureEnabled()) {
    ASSERT_EQ(candidates.size(), 3u);

    EXPECT_EQ(candidates[0].first, "This is the first paragraph.");
    EXPECT_EQ(candidates[0].second, EmbeddingPassageType::kPageContent);

    EXPECT_EQ(candidates[1].first, "This is the second paragraph.");
    EXPECT_EQ(candidates[1].second, EmbeddingPassageType::kPageContent);

    EXPECT_EQ(candidates[2].first, "The end.");
    EXPECT_EQ(candidates[2].second, EmbeddingPassageType::kPageContent);
  } else {
    EXPECT_TRUE(candidates.empty());
  }
}

// Test embeddings generation for PDF text is subject to the limit on max number
// of passages.
TEST_P(EmbeddingsCandidateGeneratorTest,
       GenerateEmbeddingsCandidatesMaxPassagesFromPDF) {
  std::string pdf_text{
      "This is the first paragraph. This is the second paragraph. Starting "
      "here, the content has exceeded the MaxPassagesFromPDF limit and will be "
      "dropped from the embeddings candidates."};

  // In production, the `page_content_passages_to_generate` argument is set to
  // the value of `MaxPassagesFromPDF` feature param.
  auto candidates = GenerateEmbeddingsCandidates(
      base::MakeRefCounted<RefCountedPDFText>(pdf_text),
      /*page_content_passages_to_generate=*/2u);

  if (IsParamFeatureEnabled()) {
    ASSERT_EQ(candidates.size(), 2u);

    EXPECT_EQ(candidates[0].first, "This is the first paragraph.");
    EXPECT_EQ(candidates[0].second, EmbeddingPassageType::kPageContent);

    EXPECT_EQ(candidates[1].first, "This is the second paragraph.");
    EXPECT_EQ(candidates[1].second, EmbeddingPassageType::kPageContent);
  } else {
    EXPECT_TRUE(candidates.empty());
  }
}

// Test embeddings generation returns an empty passages vector when PDF text
// does not contain any words.
TEST_P(EmbeddingsCandidateGeneratorTest,
       GenerateEmbeddingsCandidatesForEmptyPDFText) {
  std::string pdf_text{" \n \n\t \r \n\n \r \t\t\r \t   "};
  auto candidates = GenerateEmbeddingsCandidates(
      base::MakeRefCounted<RefCountedPDFText>(pdf_text),
      /*page_content_passages_to_generate=*/10u);

  EXPECT_TRUE(candidates.empty());
}

// Test embeddings generation returns an empty passages vector when feature
// param `MaxWordsPerAggregatePassage` is set to 0.
TEST_P(EmbeddingsCandidateGeneratorTest,
       GenerateEmbeddingsCandidatesZeroMaxWordsPerAggregatePassage) {
  // Override the feature param for maximum number of word per passage.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      passage_embeddings::kPassageEmbedder,
      {{"MaxWordsPerAggregatePassage", "0"}});

  std::string pdf_text{
      "This is the first paragraph. This is the second paragraph."};
  auto candidates = GenerateEmbeddingsCandidates(
      base::MakeRefCounted<RefCountedPDFText>(pdf_text),
      /*page_content_passages_to_generate=*/10u);

  EXPECT_TRUE(candidates.empty());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(EmbeddingsCandidateGeneratorTest);

}  // namespace

}  // namespace page_content_annotations
