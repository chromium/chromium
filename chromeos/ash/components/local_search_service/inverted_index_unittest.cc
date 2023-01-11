// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/inverted_index.h"

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/local_search_service/shared_structs.h"
#include "chromeos/ash/components/local_search_service/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::local_search_service {

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
  void SetUp() override {
    // All content weights are initialized to |kDefaultWeight|.
    index_.doc_length_ =
        std::unordered_map<std::string, uint32_t>({{"doc1", 8}, {"doc2", 6}});

    index_.dictionary_[u"A"] = PostingList(
        {{"doc1",
          Posting({WeightedPosition(kDefaultWeight, Position("header", 1, 1)),
                   WeightedPosition(kDefaultWeight, Position("header", 3, 1)),
                   WeightedPosition(kDefaultWeight, Position("body", 5, 1)),
                   WeightedPosition(kDefaultWeight, Position("body", 7, 1))})},
         {"doc2",
          Posting(
              {WeightedPosition(kDefaultWeight, Position("header", 2, 1)),
               WeightedPosition(kDefaultWeight, Position("header", 4, 1))})}});

    index_.dictionary_[u"B"] = PostingList(
        {{"doc1",
          Posting(
              {WeightedPosition(kDefaultWeight, Position("header", 2, 1)),
               WeightedPosition(kDefaultWeight, Position("body", 4, 1)),
               WeightedPosition(kDefaultWeight, Position("header", 6, 1)),
               WeightedPosition(kDefaultWeight, Position("body", 8, 1))})}});

    index_.dictionary_[u"C"] = PostingList(
        {{"doc2",
          Posting(
              {WeightedPosition(kDefaultWeight, Position("header", 1, 1)),
               WeightedPosition(kDefaultWeight, Position("body", 3, 1)),
               WeightedPosition(kDefaultWeight, Position("header", 5, 1)),
               WeightedPosition(kDefaultWeight, Position("body", 7, 1))})}});
    index_.terms_to_be_updated_.insert(u"A");
    index_.terms_to_be_updated_.insert(u"B");
    index_.terms_to_be_updated_.insert(u"C");

    // Manually set |is_index_built_| below because the docs above were not
    // added to the index using the AddOrUpdate method.
    index_.is_index_built_ = false;
  }

  PostingList FindTerm(const std::u16string& term) {
    return index_.FindTerm(term);
  }

  std::vector<Result> FindMatchingDocumentsApproximately(
      const std::unordered_set<std::u16string>& terms,
      double prefix_threshold,
      double block_threshold) {
    return index_.FindMatchingDocumentsApproximately(terms, prefix_threshold,
                                                     block_threshold);
  }

  void AddDocumentsAndCheck(const DocumentToUpdate& documents) {
    bool callback_done = false;
    index_.AddDocuments(
        documents,
        base::BindOnce([](bool* callback_done) { *callback_done = true; },
                       &callback_done));
    Wait();
    ASSERT_TRUE(callback_done);
  }

  void RemoveDocumentsAndCheck(const std::vector<std::string>& doc_ids,
                               uint32_t expect_num_deleted) {
    bool callback_done = false;
    uint32_t num_deleted = 0u;
    index_.RemoveDocuments(doc_ids,
                           base::BindOnce(
                               [](bool* callback_done, uint32_t* num_deleted,
                                  uint32_t num_deleted_callback) {
                                 *callback_done = true;
                                 *num_deleted = num_deleted_callback;
                               },
                               &callback_done, &num_deleted));
    Wait();
    ASSERT_TRUE(callback_done);
    EXPECT_EQ(num_deleted, expect_num_deleted);
  }

  void UpdateDocumentsAndCheck(const DocumentToUpdate& documents,
                               uint32_t expect_num_deleted) {
    bool callback_done = false;
    uint32_t num_deleted = 0u;
    index_.UpdateDocuments(documents,
                           base::BindOnce(
                               [](bool* callback_done, uint32_t* num_deleted,
                                  uint32_t num_deleted_callback) {
                                 *callback_done = true;
                                 *num_deleted = num_deleted_callback;
                               },
                               &callback_done, &num_deleted));
    Wait();
    ASSERT_TRUE(callback_done);
    EXPECT_EQ(num_deleted, expect_num_deleted);
  }

  void ClearInvertedIndexAndCheck() {
    bool callback_done = false;
    index_.ClearInvertedIndex(base::BindOnce(
        [](bool* callback_done) { *callback_done = true; }, &callback_done));
    Wait();
    ASSERT_TRUE(callback_done);
  }

  std::vector<TfidfResult> GetTfidf(const std::u16string& term) {
    return index_.GetTfidf(term);
  }

  bool GetTfidfForDocId(const std::u16string& term,
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

  void BuildInvertedIndexAndCheck() {
    bool callback_done = false;
    index_.BuildInvertedIndex(base::BindOnce(
        [](bool* callback_done) { *callback_done = true; }, &callback_done));
    Wait();
    ASSERT_TRUE(callback_done);
  }

  bool IsInvertedIndexBuilt() { return index_.IsInvertedIndexBuilt(); }

  std::unordered_map<std::u16string, PostingList> GetDictionary() {
    return index_.dictionary_;
  }

  std::unordered_map<std::string, uint32_t> GetDocLength() {
    return index_.doc_length_;
  }

  std::unordered_map<std::u16string, std::vector<TfidfResult>> GetTfidfCache() {
    return index_.tfidf_cache_;
  }

  DocumentToUpdate GetDocumentsToUpdate() {
    return index_.documents_to_update_;
  }

  TermSet GetTermToBeUpdated() { return index_.terms_to_be_updated_; }

  void Wait() { task_environment_.RunUntilIdle(); }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};

 private:
  InvertedIndex index_;
};

TEST_F(InvertedIndexTest, FindTermTest) {
  PostingList result = FindTerm(u"A");
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
  const std::u16string a_utf16(u"A");
  const std::u16string d_utf16(u"D");

  AddDocumentsAndCheck({{"doc3",
                         {{a_utf16,
                           {{kDefaultWeight, {"header", 1, 1}},
                            {kDefaultWeight / 2, {"body", 2, 1}},
                            {kDefaultWeight, {"header", 4, 1}}}},
                          {d_utf16,
                           {{kDefaultWeight, {"header", 3, 1}},
                            {kDefaultWeight / 2, {"body", 5, 1}}}}}}});

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
  AddDocumentsAndCheck({{"doc4",
                         {{u"E",
                           {{kDefaultWeight, {"header", 1, 1}},
                            {kDefaultWeight / 2, {"body", 2, 1}},
                            {kDefaultWeight, {"header", 4, 1}}}},
                          {u"F",
                           {{kDefaultWeight, {"header", 3, 1}},
                            {kDefaultWeight / 2, {"body", 5, 1}}}}}},
                        {"doc5",
                         {{u"E",
                           {{kDefaultWeight, {"header", 1, 1}},
                            {kDefaultWeight / 2, {"body", 2, 1}},
                            {kDefaultWeight, {"header", 4, 1}}}},
                          {u"G",
                           {{kDefaultWeight, {"header", 3, 1}},
                            {kDefaultWeight / 2, {"body", 5, 1}}}}}}});

  // Find "E"
  result = FindTerm(u"E");
  ASSERT_EQ(result.size(), 2u);

  // Find "F"
  result = FindTerm(u"F");
  ASSERT_EQ(result.size(), 1u);
}

TEST_F(InvertedIndexTest, ReplaceDocumentTest) {
  const std::u16string a_utf16(u"A");
  const std::u16string d_utf16(u"D");

  AddDocumentsAndCheck({{"doc1",
                         {{a_utf16,
                           {{kDefaultWeight, {"header", 1, 1}},
                            {kDefaultWeight / 4, {"body", 2, 1}},
                            {kDefaultWeight / 2, {"header", 4, 1}}}},
                          {d_utf16,
                           {{kDefaultWeight / 3, {"header", 3, 1}},
                            {kDefaultWeight / 5, {"body", 5, 1}}}}}}});

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
  result = FindTerm(u"B");
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

  RemoveDocumentsAndCheck({"doc1"}, 1u);

  EXPECT_EQ(GetDictionary().size(), 2u);
  EXPECT_EQ(GetDocLength().size(), 1u);
  EXPECT_EQ(GetDocLength()["doc2"], 6u);

  // Find "A"
  PostingList result = FindTerm(u"A");
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result["doc2"][0].weight, kDefaultWeight);
  EXPECT_EQ(result["doc2"][0].position.start, 2u);
  EXPECT_EQ(result["doc2"][1].weight, kDefaultWeight);
  EXPECT_EQ(result["doc2"][1].position.start, 4u);

  // Find "B"
  result = FindTerm(u"B");
  ASSERT_EQ(result.size(), 0u);

  // Find "C"
  result = FindTerm(u"C");
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result["doc2"][0].weight, kDefaultWeight);
  EXPECT_EQ(result["doc2"][0].position.start, 1u);
  EXPECT_EQ(result["doc2"][1].weight, kDefaultWeight);
  EXPECT_EQ(result["doc2"][1].position.start, 3u);
  EXPECT_EQ(result["doc2"][2].weight, kDefaultWeight);
  EXPECT_EQ(result["doc2"][2].position.start, 5u);
  EXPECT_EQ(result["doc2"][3].weight, kDefaultWeight);
  EXPECT_EQ(result["doc2"][3].position.start, 7u);

  // Removes multiple documents, but only "doc2" is actually removed since
  // "doc1" and "doc3" don't exist.
  RemoveDocumentsAndCheck({"doc1", "doc2", "doc3"}, 1u);
  EXPECT_EQ(GetDictionary().size(), 0u);
  EXPECT_EQ(GetDocLength().size(), 0u);
}

TEST_F(InvertedIndexTest, UpdateIndexTest) {
  EXPECT_EQ(GetTfidfCache().size(), 0u);
  BuildInvertedIndexAndCheck();
  EXPECT_EQ(GetTfidfCache().size(), 3u);

  // Replaces "doc1"
  AddDocumentsAndCheck({{"doc1",
                         {{u"A",
                           {{kDefaultWeight / 2, {"header", 1, 1}},
                            {kDefaultWeight / 4, {"body", 2, 1}},
                            {kDefaultWeight / 2, {"header", 4, 1}}}},
                          {u"D",
                           {{kDefaultWeight, {"header", 3, 1}},
                            {kDefaultWeight, {"body", 5, 1}}}}}}});

  EXPECT_FALSE(IsInvertedIndexBuilt());
  BuildInvertedIndexAndCheck();

  EXPECT_EQ(GetTfidfCache().size(), 3u);

  std::vector<TfidfResult> results = GetTfidf(u"A");
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

  results = GetTfidf(u"B");
  EXPECT_THAT(GetScoresFromTfidfResult(results),
              testing::UnorderedElementsAre());

  results = GetTfidf(u"C");
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

  results = GetTfidf(u"D");
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

TEST_F(InvertedIndexTest, UpdateDocumentsTest) {
  EXPECT_EQ(GetTfidfCache().size(), 0u);
  BuildInvertedIndexAndCheck();
  EXPECT_EQ(GetTfidfCache().size(), 3u);

  // Replaces "doc1" and remove "doc2"
  UpdateDocumentsAndCheck({{"doc1",
                            {{u"A",
                              {{kDefaultWeight / 2, {"header", 1, 1}},
                               {kDefaultWeight / 4, {"body", 2, 1}},
                               {kDefaultWeight / 2, {"header", 4, 1}}}},
                             {u"D",
                              {{kDefaultWeight, {"header", 3, 1}},
                               {kDefaultWeight, {"body", 5, 1}}}}}},
                           {"doc2", {}}},
                          1u);
  BuildInvertedIndexAndCheck();

  EXPECT_EQ(GetTfidfCache().size(), 2u);

  std::vector<TfidfResult> results = GetTfidf(u"C");
  EXPECT_EQ(results.size(), 0u);
}

TEST_F(InvertedIndexTest, ClearInvertedIndexTest) {
  EXPECT_EQ(GetTfidfCache().size(), 0u);
  BuildInvertedIndexAndCheck();
  EXPECT_EQ(GetTfidfCache().size(), 3u);

  // Add a document and clear the index simultaneously.
  const std::u16string a_utf16(u"A");
  const std::u16string d_utf16(u"D");
  AddDocumentsAndCheck({{"doc3",
                         {{a_utf16,
                           {{kDefaultWeight, {"header", 1, 1}},
                            {kDefaultWeight / 2, {"body", 2, 1}},
                            {kDefaultWeight, {"header", 4, 1}}}},
                          {d_utf16,
                           {{kDefaultWeight, {"header", 3, 1}},
                            {kDefaultWeight / 2, {"body", 5, 1}}}}}}});
  ClearInvertedIndexAndCheck();

  EXPECT_EQ(GetTfidfCache().size(), 0u);
  EXPECT_EQ(GetTermToBeUpdated().size(), 0u);
  EXPECT_EQ(GetDocLength().size(), 0u);
  EXPECT_EQ(GetDictionary().size(), 0u);
  EXPECT_EQ(GetDocumentsToUpdate().size(), 0u);
}

TEST_F(InvertedIndexTest, FindMatchingDocumentsApproximatelyTest) {
  const double prefix_threshold = 1.0;
  const double block_threshold = 1.0;
  const std::u16string a_utf16(u"A");
  const std::u16string b_utf16(u"B");
  const std::u16string c_utf16(u"C");
  const std::u16string d_utf16(u"D");

  // Replace doc1, same occurrences, just different weights.
  AddDocumentsAndCheck({{"doc1",
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

}  // namespace ash::local_search_service
