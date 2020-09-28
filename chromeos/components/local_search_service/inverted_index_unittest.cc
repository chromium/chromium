// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/inverted_index.h"

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/bind.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/components/local_search_service/shared_structs.h"
#include "chromeos/components/local_search_service/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace local_search_service {

namespace {

std::vector<float> GetScoresFromTfidfResult(
    const std::vector<TfidfResult>& results) {
  std::vector<float> scores;
  for (const auto& item : results) {
    scores.push_back(std::roundf(std::get<2>(item) * 100) / 100.0);
  }
  return scores;
}

constexpr double kDefaultWeight = 1.0;

}  // namespace

class InvertedIndexTest : public ::testing::Test {
 public:
  InvertedIndexTest() {
    index_.RegisterIndexBuiltCallback(base::BindRepeating(
        &InvertedIndexTest::OnIndexBuilt, base::Unretained(this)));
  }

  void SetUp() override {
    // All content weights are initialized to |kDefaultWeight|.
    index_.doc_length_ =
        std::unordered_map<std::string, uint32_t>({{"doc1", 8}, {"doc2", 6}});

    index_.dictionary_[base::UTF8ToUTF16("A")] = PostingList(
        {{"doc1",
          Posting({WeightedPosition(kDefaultWeight, Position("header", 1, 1)),
                   WeightedPosition(kDefaultWeight, Position("header", 3, 1)),
                   WeightedPosition(kDefaultWeight, Position("body", 5, 1)),
                   WeightedPosition(kDefaultWeight, Position("body", 7, 1))})},
         {"doc2",
          Posting(
              {WeightedPosition(kDefaultWeight, Position("header", 2, 1)),
               WeightedPosition(kDefaultWeight, Position("header", 4, 1))})}});

    index_.dictionary_[base::UTF8ToUTF16("B")] = PostingList(
        {{"doc1",
          Posting(
              {WeightedPosition(kDefaultWeight, Position("header", 2, 1)),
               WeightedPosition(kDefaultWeight, Position("body", 4, 1)),
               WeightedPosition(kDefaultWeight, Position("header", 6, 1)),
               WeightedPosition(kDefaultWeight, Position("body", 8, 1))})}});

    index_.dictionary_[base::UTF8ToUTF16("C")] = PostingList(
        {{"doc2",
          Posting(
              {WeightedPosition(kDefaultWeight, Position("header", 1, 1)),
               WeightedPosition(kDefaultWeight, Position("body", 3, 1)),
               WeightedPosition(kDefaultWeight, Position("header", 5, 1)),
               WeightedPosition(kDefaultWeight, Position("body", 7, 1))})}});
    index_.terms_to_be_updated_.insert(base::UTF8ToUTF16("A"));
    index_.terms_to_be_updated_.insert(base::UTF8ToUTF16("B"));
    index_.terms_to_be_updated_.insert(base::UTF8ToUTF16("C"));

    // Manually set |is_index_built_| below because the docs above were not
    // added to the index using the AddOrUpdate method.
    index_.is_index_built_ = false;
  }

  PostingList FindTerm(const base::string16& term) {
    return index_.FindTerm(term);
  }

  std::vector<Result> FindMatchingDocumentsApproximately(
      const std::unordered_set<base::string16>& terms,
      double prefix_threshold,
      double block_threshold) {
    return index_.FindMatchingDocumentsApproximately(terms, prefix_threshold,
                                                     block_threshold);
  }

  void AddDocuments(const DocumentToUpdate& documents) {
    index_.AddDocuments(documents);
  }

  void RemoveDocuments(const std::vector<std::string>& doc_ids) {
    index_.RemoveDocuments(doc_ids);
  }

  void ClearInvertedIndex() { index_.ClearInvertedIndex(); }

  std::vector<TfidfResult> GetTfidf(const base::string16& term) {
    return index_.GetTfidf(term);
  }

  bool GetTfidfForDocId(const base::string16& term,
                        const std::string& docid,
                        float* tfidf,
                        size_t* number_positions) {
    const auto& posting = GetTfidf(term);
    if (posting.empty()) {
      return false;
    }

    for (const auto& tfidf_result : posting) {
      if (std::get<0>(tfidf_result) == docid) {
        *number_positions = std::get<1>(tfidf_result).size();
        *tfidf = std::get<2>(tfidf_result);
        return true;
      }
    }
    return false;
  }

  void BuildInvertedIndex() { index_.BuildInvertedIndex(); }

  bool IsInvertedIndexBuilt() { return index_.IsInvertedIndexBuilt(); }

  std::unordered_map<base::string16, PostingList> GetDictionary() {
    return index_.dictionary_;
  }

  std::unordered_map<std::string, uint32_t> GetDocLength() {
    return index_.doc_length_;
  }

  std::unordered_map<base::string16, std::vector<TfidfResult>> GetTfidfCache() {
    return index_.tfidf_cache_;
  }

  DocumentToUpdate GetDocumentsToUpdate() {
    return index_.documents_to_update_;
  }

  TermSet GetTermToBeUpdated() { return index_.terms_to_be_updated_; }

  void Wait() { task_environment_.RunUntilIdle(); }

  bool BuildIndexCompleted() {
    return !(index_.update_in_progress_ || index_.index_building_in_progress_ ||
             index_.request_to_build_index_);
  }

  bool UpdateDocumentsCompleted() { return !index_.update_in_progress_; }

  void OnIndexBuilt() { ++num_built_; }

  int NumBuilt() { return num_built_; }

 private:
  int num_built_ = 0;
  InvertedIndex index_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
};

TEST_F(InvertedIndexTest, FindTermTest) {
  EXPECT_EQ(NumBuilt(), 0);
  PostingList result = FindTerm(base::UTF8ToUTF16("A"));
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result["doc1"][0].weight, kDefaultWeight);
  EXPECT_EQ(result["doc1"][0].position.start, 1u);
  EXPECT_EQ(result["doc1"][1].weight, kDefaultWeight);
  EXPECT_EQ(result["doc1"][1].position.start, 3u);
  EXPECT_EQ(result["doc1"][2].weight, kDefaultWeight);
  EXPECT_EQ(result["doc1"][2].position.start, 5u);
  EXPECT_EQ(result["doc1"][3].weight, kDefaultWeight);
  EXPECT_EQ(result["doc1"][3].position.start, 7u);

  EXPECT_EQ(result["doc2"][0].weight, kDefaultWeight);
  EXPECT_EQ(result["doc2"][0].position.start, 2u);
  EXPECT_EQ(result["doc2"][1].weight, kDefaultWeight);
  EXPECT_EQ(result["doc2"][1].position.start, 4u);
}

TEST_F(InvertedIndexTest, AddNewDocumentTest) {
  const base::string16 a_utf16(base::UTF8ToUTF16("A"));
  const base::string16 d_utf16(base::UTF8ToUTF16("D"));

  EXPECT_EQ(NumBuilt(), 0);
  AddDocuments({{"doc3",
                 {{a_utf16,
                   {{kDefaultWeight, {"header", 1, 1}},
                    {kDefaultWeight / 2, {"body", 2, 1}},
                    {kDefaultWeight, {"header", 4, 1}}}},
                  {d_utf16,
                   {{kDefaultWeight, {"header", 3, 1}},
                    {kDefaultWeight / 2, {"body", 5, 1}}}}}}});
  EXPECT_EQ(GetDocumentsToUpdate().size(), 0u);
  EXPECT_EQ(GetTermToBeUpdated().size(), 0u);
  Wait();
  EXPECT_EQ(NumBuilt(), 0);
  EXPECT_EQ(GetDocumentsToUpdate().size(), 0u);
  // 4 terms "A", "B", "C", "D" need to be updated.
  EXPECT_EQ(GetTermToBeUpdated().size(), 4u);
  EXPECT_TRUE(UpdateDocumentsCompleted());

  EXPECT_EQ(GetDocLength()["doc3"], 5u);

  // Find "A"
  PostingList result = FindTerm(a_utf16);
  ASSERT_EQ(result.size(), 3u);
  EXPECT_EQ(result["doc3"][0].weight, kDefaultWeight);
  EXPECT_EQ(result["doc3"][0].position.start, 1u);
  EXPECT_EQ(result["doc3"][1].weight, kDefaultWeight / 2);
  EXPECT_EQ(result["doc3"][1].position.start, 2u);
  EXPECT_EQ(result["doc3"][2].weight, kDefaultWeight);
  EXPECT_EQ(result["doc3"][2].position.start, 4u);

  // Find "D"
  result = FindTerm(d_utf16);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result["doc3"][0].weight, kDefaultWeight);
  EXPECT_EQ(result["doc3"][0].position.start, 3u);
  EXPECT_EQ(result["doc3"][1].weight, kDefaultWeight / 2);
  EXPECT_EQ(result["doc3"][1].position.start, 5u);

  // Add multiple documents
  AddDocuments({{"doc4",
                 {{base::UTF8ToUTF16("E"),
                   {{kDefaultWeight, {"header", 1, 1}},
                    {kDefaultWeight / 2, {"body", 2, 1}},
                    {kDefaultWeight, {"header", 4, 1}}}},
                  {base::UTF8ToUTF16("F"),
                   {{kDefaultWeight, {"header", 3, 1}},
                    {kDefaultWeight / 2, {"body", 5, 1}}}}}},
                {"doc5",
                 {{base::UTF8ToUTF16("E"),
                   {{kDefaultWeight, {"header", 1, 1}},
                    {kDefaultWeight / 2, {"body", 2, 1}},
                    {kDefaultWeight, {"header", 4, 1}}}},
                  {base::UTF8ToUTF16("G"),
                   {{kDefaultWeight, {"header", 3, 1}},
                    {kDefaultWeight / 2, {"body", 5, 1}}}}}}});
  EXPECT_EQ(GetDocumentsToUpdate().size(), 0u);
  EXPECT_EQ(GetTermToBeUpdated().size(), 0u);
  Wait();
  EXPECT_EQ(NumBuilt(), 0);
  EXPECT_EQ(GetDocumentsToUpdate().size(), 0u);
  // 7 terms "A", "B", "C", "D", "E", "F", "G" need to be updated.
  EXPECT_EQ(GetTermToBeUpdated().size(), 7u);
  EXPECT_TRUE(UpdateDocumentsCompleted());

  // Find "E"
  result = FindTerm(base::UTF8ToUTF16("E"));
  ASSERT_EQ(result.size(), 2u);

  // Find "F"
  result = FindTerm(base::UTF8ToUTF16("F"));
  ASSERT_EQ(result.size(), 1u);
}

TEST_F(InvertedIndexTest, ReplaceDocumentTest) {
  const base::string16 a_utf16(base::UTF8ToUTF16("A"));
  const base::string16 d_utf16(base::UTF8ToUTF16("D"));

  EXPECT_EQ(NumBuilt(), 0);
  AddDocuments({{"doc1",
                 {{a_utf16,
                   {{kDefaultWeight, {"header", 1, 1}},
                    {kDefaultWeight / 4, {"body", 2, 1}},
                    {kDefaultWeight / 2, {"header", 4, 1}}}},
                  {d_utf16,
                   {{kDefaultWeight / 3, {"header", 3, 1}},
                    {kDefaultWeight / 5, {"body", 5, 1}}}}}}});
  EXPECT_EQ(GetDocumentsToUpdate().size(), 0u);
  Wait();
  EXPECT_EQ(NumBuilt(), 0);
  EXPECT_TRUE(UpdateDocumentsCompleted());

  EXPECT_EQ(GetDocLength()["doc1"], 5u);
  EXPECT_EQ(GetDocLength()["doc2"], 6u);

  // Find "A"
  PostingList result = FindTerm(a_utf16);
  ASSERT_EQ(result.size(), 2u);
  EXPECT_EQ(result["doc1"][0].weight, kDefaultWeight);
  EXPECT_EQ(result["doc1"][0].position.start, 1u);
  EXPECT_EQ(result["doc1"][1].weight, kDefaultWeight / 4);
  EXPECT_EQ(result["doc1"][1].position.start, 2u);
  EXPECT_EQ(result["doc1"][2].weight, kDefaultWeight / 2);
  EXPECT_EQ(result["doc1"][2].position.start, 4u);

  // Find "B"
  result = FindTerm(base::UTF8ToUTF16("B"));
  ASSERT_EQ(result.size(), 0u);

  // Find "D"
  result = FindTerm(d_utf16);
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result["doc1"][0].weight, kDefaultWeight / 3);
  EXPECT_EQ(result["doc1"][0].position.start, 3u);
  EXPECT_EQ(result["doc1"][1].weight, kDefaultWeight / 5);
  EXPECT_EQ(result["doc1"][1].position.start, 5u);
}

TEST_F(InvertedIndexTest, RemoveDocumentTest) {
  EXPECT_EQ(GetDictionary().size(), 3u);
  EXPECT_EQ(GetDocLength().size(), 2u);

  EXPECT_EQ(NumBuilt(), 0);
  RemoveDocuments({"doc1"});
  Wait();
  EXPECT_EQ(NumBuilt(), 0);
  EXPECT_TRUE(UpdateDocumentsCompleted());

  EXPECT_EQ(GetDictionary().size(), 2u);
  EXPECT_EQ(GetDocLength().size(), 1u);
  EXPECT_EQ(GetDocLength()["doc2"], 6u);

  // Find "A"
  PostingList result = FindTerm(base::UTF8ToUTF16("A"));
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result["doc2"][0].weight, kDefaultWeight);
  EXPECT_EQ(result["doc2"][0].position.start, 2u);
  EXPECT_EQ(result["doc2"][1].weight, kDefaultWeight);
  EXPECT_EQ(result["doc2"][1].position.start, 4u);

  // Find "B"
  result = FindTerm(base::UTF8ToUTF16("B"));
  ASSERT_EQ(result.size(), 0u);

  // Find "C"
  result = FindTerm(base::UTF8ToUTF16("C"));
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result["doc2"][0].weight, kDefaultWeight);
  EXPECT_EQ(result["doc2"][0].position.start, 1u);
  EXPECT_EQ(result["doc2"][1].weight, kDefaultWeight);
  EXPECT_EQ(result["doc2"][1].position.start, 3u);
  EXPECT_EQ(result["doc2"][2].weight, kDefaultWeight);
  EXPECT_EQ(result["doc2"][2].position.start, 5u);
  EXPECT_EQ(result["doc2"][3].weight, kDefaultWeight);
  EXPECT_EQ(result["doc2"][3].position.start, 7u);

  // Removes multiple documents
  RemoveDocuments({"doc1", "doc2", "doc3"});
  Wait();
  EXPECT_EQ(NumBuilt(), 0);
  EXPECT_TRUE(UpdateDocumentsCompleted());
  EXPECT_EQ(GetDictionary().size(), 0u);
  EXPECT_EQ(GetDocLength().size(), 0u);
}

TEST_F(InvertedIndexTest, TfidfFromZeroTest) {
  EXPECT_EQ(GetTfidfCache().size(), 0u);
  EXPECT_FALSE(IsInvertedIndexBuilt());
  EXPECT_EQ(NumBuilt(), 0);
  BuildInvertedIndex();
  Wait();
  EXPECT_EQ(NumBuilt(), 1);
  EXPECT_TRUE(BuildIndexCompleted());

  std::vector<TfidfResult> results = GetTfidf(base::UTF8ToUTF16("A"));
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre(0.5, 0.33));

  results = GetTfidf(base::UTF8ToUTF16("B"));
  EXPECT_EQ(results.size(), 1u);
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre(0.7));

  results = GetTfidf(base::UTF8ToUTF16("C"));
  EXPECT_EQ(results.size(), 1u);
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre(0.94));

  results = GetTfidf(base::UTF8ToUTF16("D"));
  EXPECT_EQ(results.size(), 0u);
}

TEST_F(InvertedIndexTest, UpdateIndexTest) {
  EXPECT_EQ(GetTfidfCache().size(), 0u);
  BuildInvertedIndex();
  Wait();
  EXPECT_EQ(NumBuilt(), 1);
  EXPECT_TRUE(BuildIndexCompleted());

  EXPECT_TRUE(IsInvertedIndexBuilt());
  EXPECT_EQ(GetTfidfCache().size(), 3u);

  // Replaces "doc1"
  AddDocuments({{"doc1",
                 {{base::UTF8ToUTF16("A"),
                   {{kDefaultWeight / 2, {"header", 1, 1}},
                    {kDefaultWeight / 4, {"body", 2, 1}},
                    {kDefaultWeight / 2, {"header", 4, 1}}}},
                  {base::UTF8ToUTF16("D"),
                   {{kDefaultWeight, {"header", 3, 1}},
                    {kDefaultWeight, {"body", 5, 1}}}}}}});
  EXPECT_EQ(GetDocumentsToUpdate().size(), 0u);
  Wait();
  EXPECT_EQ(NumBuilt(), 1);
  EXPECT_TRUE(UpdateDocumentsCompleted());

  EXPECT_FALSE(IsInvertedIndexBuilt());
  BuildInvertedIndex();
  Wait();
  EXPECT_EQ(NumBuilt(), 2);
  EXPECT_TRUE(BuildIndexCompleted());

  EXPECT_EQ(GetTfidfCache().size(), 3u);

  std::vector<TfidfResult> results = GetTfidf(base::UTF8ToUTF16("A"));
  const double expected_tfidf_A_doc1 =
      std::roundf(
          TfIdfScore(
              /*num_docs=*/2,
              /*num_docs_with_term=*/2,
              /*weighted_num_term_occurrence_in_doc=*/kDefaultWeight / 2 +
                  kDefaultWeight / 4 + kDefaultWeight / 2,
              /*doc_length=*/5) *
          100) /
      100;
  const double expected_tfidf_A_doc2 =
      std::roundf(
          TfIdfScore(/*num_docs=*/2,
                     /*num_docs_with_term=*/2,
                     /*weighted_num_term_occurrence_in_doc=*/kDefaultWeight * 2,
                     /*doc_length=*/6) *
          100) /
      100;
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre(expected_tfidf_A_doc1,
                                            expected_tfidf_A_doc2));

  results = GetTfidf(base::UTF8ToUTF16("B"));
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre());

  results = GetTfidf(base::UTF8ToUTF16("C"));
  const double expected_tfidf_C_doc2 =
      std::roundf(
          TfIdfScore(/*num_docs=*/2,
                     /*num_docs_with_term=*/1,
                     /*weighted_num_term_occurrence_in_doc=*/kDefaultWeight * 4,
                     /*doc_length=*/6) *
          100) /
      100;
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre(expected_tfidf_C_doc2));

  results = GetTfidf(base::UTF8ToUTF16("D"));
  const double expected_tfidf_D_doc1 =
      std::roundf(
          TfIdfScore(/*num_docs=*/2,
                     /*num_docs_with_term=*/1,
                     /*weighted_num_term_occurrence_in_doc=*/kDefaultWeight * 2,
                     /*doc_length=*/5) *
          100) /
      100;
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre(expected_tfidf_D_doc1));
}

TEST_F(InvertedIndexTest, ClearInvertedIndexTest) {
  EXPECT_EQ(GetTfidfCache().size(), 0u);
  BuildInvertedIndex();
  Wait();
  EXPECT_EQ(NumBuilt(), 1);
  EXPECT_TRUE(BuildIndexCompleted());

  EXPECT_TRUE(IsInvertedIndexBuilt());
  EXPECT_EQ(GetTfidfCache().size(), 3u);

  // Add a document and clear the index simultaneously.
  const base::string16 a_utf16(base::UTF8ToUTF16("A"));
  const base::string16 d_utf16(base::UTF8ToUTF16("D"));
  AddDocuments({{"doc3",
                 {{a_utf16,
                   {{kDefaultWeight, {"header", 1, 1}},
                    {kDefaultWeight / 2, {"body", 2, 1}},
                    {kDefaultWeight, {"header", 4, 1}}}},
                  {d_utf16,
                   {{kDefaultWeight, {"header", 3, 1}},
                    {kDefaultWeight / 2, {"body", 5, 1}}}}}}});
  ClearInvertedIndex();
  Wait();

  EXPECT_EQ(GetTfidfCache().size(), 0u);
  EXPECT_EQ(GetTermToBeUpdated().size(), 0u);
  EXPECT_EQ(GetDocLength().size(), 0u);
  EXPECT_EQ(GetDictionary().size(), 0u);
  EXPECT_EQ(GetDocumentsToUpdate().size(), 0u);
}

TEST_F(InvertedIndexTest, FindMatchingDocumentsApproximatelyTest) {
  const double prefix_threshold = 1.0;
  const double block_threshold = 1.0;
  const base::string16 a_utf16(base::UTF8ToUTF16("A"));
  const base::string16 b_utf16(base::UTF8ToUTF16("B"));
  const base::string16 c_utf16(base::UTF8ToUTF16("C"));
  const base::string16 d_utf16(base::UTF8ToUTF16("D"));

  EXPECT_EQ(NumBuilt(), 0);

  // Replace doc1, same occurrences, just different weights.
  AddDocuments({{"doc1",
                 {{a_utf16,
                   {{kDefaultWeight, {"header", 1, 1}},
                    {kDefaultWeight, {"header", 3, 1}},
                    {kDefaultWeight / 2, {"body", 5, 1}},
                    {kDefaultWeight / 2, {"body", 7, 1}}}},
                  {b_utf16,
                   {{kDefaultWeight / 2, {"header", 2, 1}},
                    {kDefaultWeight / 2, {"header", 6, 1}},
                    {kDefaultWeight / 3, {"body", 4, 1}},
                    {kDefaultWeight / 3, {"body", 5, 1}}}}}}});
  EXPECT_EQ(GetDocumentsToUpdate().size(), 0u);
  Wait();
  EXPECT_EQ(NumBuilt(), 0);
  EXPECT_TRUE(UpdateDocumentsCompleted());

  BuildInvertedIndex();
  Wait();
  EXPECT_EQ(NumBuilt(), 1);
  EXPECT_TRUE(BuildIndexCompleted());

  {
    // "A" exists in "doc1" and "doc2". The score of each document is simply A's
    // TF-IDF score.
    const std::vector<Result> matching_docs =
        FindMatchingDocumentsApproximately({a_utf16}, prefix_threshold,
                                           block_threshold);

    EXPECT_EQ(matching_docs.size(), 2u);
    std::vector<std::string> expected_docids = {"doc1", "doc2"};
    // TODO(jiameng): for testing, we should  use another independent method to
    // verify scores.
    std::vector<float> actual_scores;
    for (size_t i = 0; i < 2; ++i) {
      const auto& docid = expected_docids[i];
      float expected_score = 0;
      size_t expected_num_pos = 0u;
      EXPECT_TRUE(
          GetTfidfForDocId(a_utf16, docid, &expected_score, &expected_num_pos));
      CheckResult(matching_docs[i], docid, expected_score, expected_num_pos);
      actual_scores.push_back(expected_score);
    }

    // Check scores are non-increasing.
    EXPECT_GE(actual_scores[0], actual_scores[1]);
  }

  {
    // "D" does not exist in the index.
    const std::vector<Result> matching_docs =
        FindMatchingDocumentsApproximately({d_utf16}, prefix_threshold,
                                           block_threshold);

    EXPECT_TRUE(matching_docs.empty());
  }

  {
    // Query is {"A", "B", "C"}, which matches all documents.
    const std::vector<Result> matching_docs =
        FindMatchingDocumentsApproximately({a_utf16, b_utf16, c_utf16},
                                           prefix_threshold, block_threshold);
    EXPECT_EQ(matching_docs.size(), 2u);

    // Ranked documents are {"doc2", "doc1"}.
    // "doc2" score comes from sum of TF-IDF of "A" and "C"
    float expected_score2_A = 0;
    size_t expected_num_pos2_A = 0u;
    EXPECT_TRUE(GetTfidfForDocId(a_utf16, "doc2", &expected_score2_A,
                                 &expected_num_pos2_A));
    float expected_score2_C = 0;
    size_t expected_num_pos2_C = 0u;
    EXPECT_TRUE(GetTfidfForDocId(c_utf16, "doc2", &expected_score2_C,
                                 &expected_num_pos2_C));
    const float expected_score2 = expected_score2_A + expected_score2_C;
    const size_t expected_num_pos2 = expected_num_pos2_A + expected_num_pos2_C;
    CheckResult(matching_docs[0], "doc2", expected_score2, expected_num_pos2);

    // "doc1" score comes from sum of TF-IDF of "A" and "B".
    float expected_score1_A = 0;
    size_t expected_num_pos1_A = 0u;
    EXPECT_TRUE(GetTfidfForDocId(a_utf16, "doc1", &expected_score1_A,
                                 &expected_num_pos1_A));
    float expected_score1_B = 0;
    size_t expected_num_pos1_B = 0u;
    EXPECT_TRUE(GetTfidfForDocId(b_utf16, "doc1", &expected_score1_B,
                                 &expected_num_pos1_B));
    const float expected_score1 = expected_score1_A + expected_score1_B;
    const size_t expected_num_pos1 = expected_num_pos1_B + expected_num_pos1_B;
    CheckResult(matching_docs[1], "doc1", expected_score1, expected_num_pos1);

    EXPECT_GE(expected_score2, expected_score1);
  }
}

}  // namespace local_search_service
}  // namespace chromeos
