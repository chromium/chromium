// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/content_annotator/content_annotator_semantic_match_classifier.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/passage_embeddings/core/passage_embeddings_test_util.h"
#include "components/passage_embeddings/core/passage_embeddings_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

namespace {

using ::testing::_;

using Embedding = passage_embeddings::Embedding;
using TaskId = passage_embeddings::Embedder::TaskId;

constexpr TaskId kTaskId = 0;

class MockEmbedder : public passage_embeddings::TestEmbedder {
 public:
  MockEmbedder() = default;
  ~MockEmbedder() override = default;

  MOCK_METHOD(TaskId,
              ComputePassagesEmbeddings,
              (passage_embeddings::PassagePriority priority,
               std::vector<std::string> passages,
               ComputePassagesEmbeddingsCallback callback),
              (override));
};

}  // namespace

class ContentAnnotatorSemanticMatchClassifierTest : public testing::Test {
 public:
  ContentAnnotatorSemanticMatchClassifierTest() = default;
  ~ContentAnnotatorSemanticMatchClassifierTest() override = default;

 protected:
  void SetUp() override { mock_embedder_ = std::make_unique<MockEmbedder>(); }

  std::unique_ptr<ContentAnnotatorSemanticMatchClassifier> CreateClassifier(
      std::string_view rules_json) {
    base::test::TestFuture<
        std::unique_ptr<ContentAnnotatorSemanticMatchClassifier>>
        future;

    CreateSemanticMatchClassifier(rules_json, mock_embedder_.get(),
                                  future.GetCallback());

    return future.Take();
  }

  void MockEmbedderResponse(
      const std::map<std::string, std::vector<float>>& keyword_to_embedding,
      std::vector<std::string>* captured_passages = nullptr) {
    EXPECT_CALL(*mock_embedder_, ComputePassagesEmbeddings(_, _, _))
        .WillOnce(
            [keyword_to_embedding, captured_passages](
                passage_embeddings::PassagePriority,
                std::vector<std::string> passages,
                passage_embeddings::Embedder::ComputePassagesEmbeddingsCallback
                    callback) {
              if (captured_passages) {
                *captured_passages = passages;
              }
              std::vector<Embedding> embeddings;
              for (const auto& passage : passages) {
                auto it = keyword_to_embedding.find(passage);
                CHECK(it != keyword_to_embedding.end());
                Embedding embedding(it->second);
                embedding.Normalize();
                embeddings.push_back(std::move(embedding));
              }
              std::move(callback).Run(
                  passages, std::move(embeddings), kTaskId,
                  passage_embeddings::ComputeEmbeddingsStatus::kSuccess);
              return kTaskId;
            });
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<MockEmbedder> mock_embedder_;
};

TEST_F(ContentAnnotatorSemanticMatchClassifierTest,
       CreateFailsWithInvalidJson) {
  auto classifier = CreateClassifier("invalid json");
  EXPECT_FALSE(classifier);
}

TEST_F(ContentAnnotatorSemanticMatchClassifierTest, FailsWithNullEmbedder) {
  const char kRules[] = R"JSON({ "category1": ["keyword1"] })JSON";
  base::test::TestFuture<
      std::unique_ptr<ContentAnnotatorSemanticMatchClassifier>>
      future;
  CreateSemanticMatchClassifier(kRules, nullptr, future.GetCallback());
  EXPECT_FALSE(future.Take());
}

TEST_F(ContentAnnotatorSemanticMatchClassifierTest, CreateSucceeds) {
  const char kRules[] = R"JSON({
    "category1": ["keyword1", "keyword2"],
    "category2": ["keyword3"]
  })JSON";

  // Mock response for TryInitialize.
  MockEmbedderResponse({
      {"keyword1", {1.0f, 0.0f}},
      {"keyword2", {0.8f, 0.6f}},
      {"keyword3", {0.0f, 1.0f}},
  });

  auto classifier = CreateClassifier(kRules);
  EXPECT_TRUE(classifier);
}

TEST_F(ContentAnnotatorSemanticMatchClassifierTest, Classify) {
  const char kRules[] = R"JSON({
    "category1": ["cat"],
    "category2": ["dog"]
  })JSON";

  std::vector<std::string> captured_passages;
  MockEmbedderResponse(
      {
          {"cat", {1.0f, 0.0f}},
          {"dog", {0.0f, 1.0f}},
      },
      &captured_passages);

  auto classifier = CreateClassifier(kRules);
  ASSERT_TRUE(classifier);

  // Strong match for category1.
  Embedding cat_embedding(std::vector<float>{1.0f, 0.01f});
  auto cat_result = classifier->Classify(cat_embedding);
  EXPECT_THAT(captured_passages, testing::UnorderedElementsAre("cat", "dog"));
  ASSERT_TRUE(cat_result.has_value());
  EXPECT_EQ(cat_result->category, "category1");
  EXPECT_EQ(cat_result->score, 1.0f);

  // Strong match for category2.
  Embedding dog_embedding(std::vector<float>{0.01f, 1.0f});
  auto dog_result = classifier->Classify(dog_embedding);
  ASSERT_TRUE(dog_result.has_value());
  EXPECT_EQ(dog_result->category, "category2");
  EXPECT_EQ(dog_result->score, 1.0f);

  // Equidistant, should pick one.
  Embedding equidistant_embedding(std::vector<float>{1.0f, 1.0f});
  // The exact result depends on iteration order, but it should be one of them.
  auto equidistant_result = classifier->Classify(equidistant_embedding);
  ASSERT_TRUE(equidistant_result.has_value());
  EXPECT_THAT(equidistant_result->category, "category1");
}

TEST_F(ContentAnnotatorSemanticMatchClassifierTest, ClassifyWithNoMatch) {
  const char kRules[] = R"JSON({ "category1": ["keyword1"] })JSON";
  MockEmbedderResponse({{"keyword1", {1.0f, 0.0f, 0.0f}}});
  auto classifier = CreateClassifier(kRules);
  ASSERT_TRUE(classifier);

  Embedding non_matching_embedding(std::vector<float>{0.0f, 1.0f, 0.0f});
  EXPECT_FALSE(classifier->Classify(non_matching_embedding).has_value());
}

TEST_F(ContentAnnotatorSemanticMatchClassifierTest, ClassifyWithZeroMagnitude) {
  const char kRules[] = R"JSON({ "category1": ["keyword1"] })JSON";
  // Rule embedding has 3 dimensions.
  MockEmbedderResponse({{"keyword1", {1.0f, 0.0f, 0.0f}}});
  auto classifier = CreateClassifier(kRules);
  ASSERT_TRUE(classifier);

  // Input embedding has 3 dimensions but zero magnitude.
  Embedding zero_magnitude_embedding(std::vector<float>{0.0f, 0.0f, 0.0f});
  EXPECT_FALSE(classifier->Classify(zero_magnitude_embedding).has_value());
}

}  // namespace accessibility_annotator
