// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/inverted_index_search.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/ash/components/local_search_service/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::local_search_service {

namespace {

// This is (data-id, content-ids).
using ResultWithIds = std::pair<std::string, std::vector<std::string>>;

// (content-id, content).
using ContentWithId = std::pair<std::string, std::string>;

// (content-id, content, weight).
using WeightedContentWithId = std::tuple<std::string, std::string, float>;

// (document-id, number-of-occurrences).
using TermOccurrence = std::vector<std::pair<std::string, uint32_t>>;

void GetSizeAndCheckResults(InvertedIndexSearch* index,
                            base::test::TaskEnvironment* task_environment,
                            uint32_t expectd_num_items) {
  DCHECK(index);
  bool callback_done = false;
  uint32_t num_items = 0;
  index->GetSize(base::BindOnce(
      [](bool* callback_done, uint32_t* num_items, uint64_t size) {
        *callback_done = true;
        *num_items = size;
      },
      &callback_done, &num_items));
  task_environment->RunUntilIdle();
  ASSERT_TRUE(callback_done);
  EXPECT_EQ(num_items, expectd_num_items);
}

void AddOrUpdate(InvertedIndexSearch* index,
                 base::test::TaskEnvironment* task_environment,
                 const std::vector<Data>& data) {
  DCHECK(index);
  bool callback_done = false;
  index->AddOrUpdate(
      data, base::BindOnce([](bool* callback_done) { *callback_done = true; },
                           &callback_done));
  task_environment->RunUntilIdle();
  ASSERT_TRUE(callback_done);
}

void Delete(InvertedIndexSearch* index,
            base::test::TaskEnvironment* task_environment,
            const std::vector<std::string>& ids,
            uint32_t expect_num_deleted) {
  DCHECK(index);
  bool callback_done = false;
  uint32_t num_deleted = 0u;
  index->Delete(ids, base::BindOnce(
                         [](bool* callback_done, uint32_t* num_deleted,
                            uint32_t num_deleted_callback) {
                           *callback_done = true;
                           *num_deleted = num_deleted_callback;
                         },
                         &callback_done, &num_deleted));
  task_environment->RunUntilIdle();
  ASSERT_TRUE(callback_done);
  EXPECT_EQ(num_deleted, expect_num_deleted);
}

void UpdateDocuments(InvertedIndexSearch* index,
                     base::test::TaskEnvironment* task_environment,
                     const std::vector<Data>& data,
                     uint32_t expect_num_deleted) {
  DCHECK(index);
  bool callback_done = false;
  uint32_t num_deleted = 0u;
  index->UpdateDocuments(data,
                         base::BindOnce(
                             [](bool* callback_done, uint32_t* num_deleted,
                                uint32_t num_deleted_callback) {
                               *callback_done = true;
                               *num_deleted = num_deleted_callback;
                             },
                             &callback_done, &num_deleted));
  task_environment->RunUntilIdle();
  ASSERT_TRUE(callback_done);
  EXPECT_EQ(num_deleted, expect_num_deleted);
}

std::vector<Result> Find(InvertedIndexSearch* index,
                         base::test::TaskEnvironment* task_environment,
                         std::string query,
                         int32_t max_results,
                         ResponseStatus expected_status) {
  DCHECK(index);
  bool callback_done = false;
  ResponseStatus status;
  std::vector<Result> results;

  index->Find(
      base::UTF8ToUTF16(query), max_results,
      base::BindOnce(
          [](bool* callback_done, ResponseStatus* status,
             std::vector<Result>* results, ResponseStatus status_callback,
             const std::optional<std::vector<Result>>& results_callback) {
            *callback_done = true;
            *status = status_callback;
            if (results_callback.has_value())
              *results = results_callback.value();
          },
          &callback_done, &status, &results));
  task_environment->RunUntilIdle();
  EXPECT_TRUE(callback_done);
  EXPECT_EQ(status, expected_status);
  return results;
}

}  // namespace

class InvertedIndexSearchTest : public testing::Test {
 public:
  void SetUp() override {
    search_ = std::make_unique<InvertedIndexSearch>(IndexId::kCrosSettings);
  }
  void Wait() { task_environment_.RunUntilIdle(); }

 protected:
  std::unique_ptr<InvertedIndexSearch> search_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
};

TEST_F(InvertedIndexSearchTest, Add) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1",
       {{"cid_1", "This is a help wi-fi article"},
        {"cid_2", "Another help help wi-fi"}}},
      {"id2", {{"cid_3", "help article on wi-fi"}}}};

  const std::vector<Data> data = CreateTestData(data_to_register);
  AddOrUpdate(search_.get(), &task_environment_, data);
  GetSizeAndCheckResults(search_.get(), &task_environment_, 2u);

  {
    // "network" does not exist in the index.
    const TermOccurrence doc_with_freq =
        search_->FindTermForTesting(u"network");
    EXPECT_TRUE(doc_with_freq.empty());
  }

  {
    // "help" exists in the index.
    const TermOccurrence doc_with_freq = search_->FindTermForTesting(u"help");
    EXPECT_EQ(doc_with_freq.size(), 2u);
    EXPECT_EQ(doc_with_freq[0].first, "id1");
    EXPECT_EQ(doc_with_freq[0].second, 3u);
    EXPECT_EQ(doc_with_freq[1].first, "id2");
    EXPECT_EQ(doc_with_freq[1].second, 1u);
  }

  {
    // "wifi" exists in the index but "wi-fi" doesn't because of normalization.
    TermOccurrence doc_with_freq = search_->FindTermForTesting(u"wifi");
    EXPECT_EQ(doc_with_freq.size(), 2u);
    EXPECT_EQ(doc_with_freq[0].first, "id1");
    EXPECT_EQ(doc_with_freq[0].second, 2u);
    EXPECT_EQ(doc_with_freq[1].first, "id2");
    EXPECT_EQ(doc_with_freq[1].second, 1u);

    doc_with_freq = search_->FindTermForTesting(u"wi-fi");
    EXPECT_TRUE(doc_with_freq.empty());

    // "WiFi" doesn't exist because the index stores normalized word.
    doc_with_freq = search_->FindTermForTesting(u"WiFi");
    EXPECT_TRUE(doc_with_freq.empty());
  }

  {
    // "this" does not exist in the index because it's a stopword
    const TermOccurrence doc_with_freq = search_->FindTermForTesting(u"this");
    EXPECT_TRUE(doc_with_freq.empty());
  }
}

TEST_F(InvertedIndexSearchTest, Update) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1",
       {{"cid_1", "This is a help wi-fi article"},
        {"cid_2", "Another help help wi-fi"}}},
      {"id2", {{"cid_3", "help article on wi-fi"}}}};

  const std::vector<Data> data = CreateTestData(data_to_register);
  AddOrUpdate(search_.get(), &task_environment_, data);
  GetSizeAndCheckResults(search_.get(), &task_environment_, 2u);

  const std::map<std::string, std::vector<ContentWithId>> data_to_update = {
      {"id1",
       {{"cid_1", "This is a help bluetooth article"},
        {"cid_2", "Google Playstore Google Music"}}},
      {"id3", {{"cid_3", "Google Map"}}}};

  const std::vector<Data> updated_data = CreateTestData(data_to_update);
  AddOrUpdate(search_.get(), &task_environment_, updated_data);
  GetSizeAndCheckResults(search_.get(), &task_environment_, 3u);

  {
    const TermOccurrence doc_with_freq =
        search_->FindTermForTesting(u"bluetooth");
    EXPECT_EQ(doc_with_freq.size(), 1u);
    EXPECT_EQ(doc_with_freq[0].first, "id1");
    EXPECT_EQ(doc_with_freq[0].second, 1u);
  }

  {
    const TermOccurrence doc_with_freq = search_->FindTermForTesting(u"wifi");
    EXPECT_EQ(doc_with_freq.size(), 1u);
    EXPECT_EQ(doc_with_freq[0].first, "id2");
    EXPECT_EQ(doc_with_freq[0].second, 1u);
  }

  {
    const TermOccurrence doc_with_freq = search_->FindTermForTesting(u"google");
    EXPECT_EQ(doc_with_freq.size(), 2u);
    EXPECT_EQ(doc_with_freq[0].first, "id1");
    EXPECT_EQ(doc_with_freq[0].second, 2u);
    EXPECT_EQ(doc_with_freq[1].first, "id3");
    EXPECT_EQ(doc_with_freq[1].second, 1u);
  }
}

TEST_F(InvertedIndexSearchTest, Delete) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1",
       {{"cid_1", "This is a help wi-fi article"},
        {"cid_2", "Another help help wi-fi"}}},
      {"id2", {{"cid_3", "help article on wi-fi"}}}};

  const std::vector<Data> data = CreateTestData(data_to_register);
  AddOrUpdate(search_.get(), &task_environment_, data);
  GetSizeAndCheckResults(search_.get(), &task_environment_, 2u);

  Delete(search_.get(), &task_environment_, {"id1"}, 1u);

  {
    const TermOccurrence doc_with_freq = search_->FindTermForTesting(u"wifi");
    EXPECT_EQ(doc_with_freq.size(), 1u);
    EXPECT_EQ(doc_with_freq[0].first, "id2");
    EXPECT_EQ(doc_with_freq[0].second, 1u);
  }
}

TEST_F(InvertedIndexSearchTest, ClearIndex) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1",
       {{"cid_1", "This is a help wi-fi article"},
        {"cid_2", "Another help help wi-fi"}}},
      {"id2", {{"cid_3", "help article on wi-fi"}}}};

  const std::vector<Data> data = CreateTestData(data_to_register);

  AddOrUpdate(search_.get(), &task_environment_, data);
  GetSizeAndCheckResults(search_.get(), &task_environment_, 2u);

  bool callback_done = false;
  search_->ClearIndex(base::BindOnce(
      [](bool* callback_done) { *callback_done = true; }, &callback_done));
  Wait();
  ASSERT_TRUE(callback_done);
  GetSizeAndCheckResults(search_.get(), &task_environment_, 0u);
}

TEST_F(InvertedIndexSearchTest, FindTest) {
  const std::map<std::string, std::vector<WeightedContentWithId>>
      data_to_register = {{"id1",
                           {{"cid_1", "This is a help wi-fi article", 0.8},
                            {"cid_2", "Another help help wi-fi", 0.6}}},
                          {"id2", {{"cid_3", "help article on wi-fi", 0.6}}}};
  const std::vector<Data> data = CreateTestData(data_to_register);

  // Nothing has been added to the index.
  std::vector<Result> results =
      Find(search_.get(), &task_environment_, "network",
           /*max_results=*/10, ResponseStatus::kEmptyIndex);
  EXPECT_TRUE(results.empty());

  // Data is added and then deleted from index, making the index empty.
  AddOrUpdate(search_.get(), &task_environment_, data);
  GetSizeAndCheckResults(search_.get(), &task_environment_, 2u);
  Delete(search_.get(), &task_environment_, {"id1", "id2"}, 2u);
  GetSizeAndCheckResults(search_.get(), &task_environment_, 0u);

  results = Find(search_.get(), &task_environment_, "network",
                 /*max_results=*/10, ResponseStatus::kEmptyIndex);
  EXPECT_TRUE(results.empty());

  // Index is populated again, but query is empty.
  AddOrUpdate(search_.get(), &task_environment_, data);
  GetSizeAndCheckResults(search_.get(), &task_environment_, 2u);

  results = Find(search_.get(), &task_environment_, "", /*max_results=*/10,
                 ResponseStatus::kEmptyQuery);
  EXPECT_TRUE(results.empty());

  // No document is found for a given query.
  results = Find(search_.get(), &task_environment_, "networkstuff",
                 /*max_results=*/10, ResponseStatus::kSuccess);
  EXPECT_TRUE(results.empty());

  {
    // A document is found.
    // Query's case is normalized.
    results = Find(search_.get(), &task_environment_, "ANOTHER networkstuff",
                   /*max_results=*/10, ResponseStatus::kSuccess);
    EXPECT_EQ(results.size(), 1u);

    // "another" only exists in "id1".
    const float expected_score =
        TfIdfScore(/*num_docs=*/2,
                   /*num_docs_with_term=*/1,
                   /*weighted_num_term_occurrence_in_doc=*/0.6,
                   /*doc_length=*/7);
    CheckResult(results[0], "id1", expected_score,
                /*expected_number_positions=*/1);
  }

  {
    // Two documents are found.
    results = Find(search_.get(), &task_environment_, "another help",
                   /*max_results=*/10, ResponseStatus::kSuccess);
    EXPECT_EQ(results.size(), 2u);

    // "id1" score comes from both "another" and "help".
    const float expected_score_id1 =
        TfIdfScore(/*num_docs=*/2,
                   /*num_docs_with_term=*/1,
                   /*weighted_num_term_occurrence_in_doc=*/0.6,
                   /*doc_length=*/7) +
        TfIdfScore(/*num_docs=*/2,
                   /*num_docs_with_term=*/2,
                   /*weighted_num_term_occurrence_in_doc=*/0.8 + 0.6 * 2,
                   /*doc_length=*/7);
    // "id2" score comes "help".
    const float expected_score_id2 =
        TfIdfScore(/*num_docs=*/2,
                   /*num_docs_with_term=*/2,
                   /*weighted_num_term_occurrence_in_doc=*/0.6,
                   /*doc_length=*/3);

    EXPECT_GE(expected_score_id1, expected_score_id2);
    CheckResult(results[0], "id1", expected_score_id1,
                /*expected_number_positions=*/4);
    CheckResult(results[1], "id2", expected_score_id2,
                /*expected_number_positions=*/1);
  }

  {
    // Same as above,  but max number of results is set to 1.
    results = Find(search_.get(), &task_environment_, "another help",
                   /*max_results=*/1, ResponseStatus::kSuccess);
    EXPECT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].id, "id1");
  }

  {
    // Same as above, but set max_results to 0, meaning no max.
    results = Find(search_.get(), &task_environment_, "another help",
                   /*max_results=*/0, ResponseStatus::kSuccess);
    EXPECT_EQ(results.size(), 2u);
  }
}

TEST_F(InvertedIndexSearchTest, SequenceOfDeletes) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1",
       {{"cid_1", "This is a help wi-fi article"},
        {"cid_2", "Another help help wi-fi"}}},
      {"id2", {{"cid_3", "help article on wi-fi"}}}};

  const std::vector<Data> data = CreateTestData(data_to_register);
  AddOrUpdate(search_.get(), &task_environment_, data);

  const std::map<std::string, std::vector<ContentWithId>> data_to_update = {
      {"id1",
       {{"cid_1", "This is a help bluetooth article"},
        {"cid_2", "Google Playstore Google Music"}}},
      {"id3", {{"cid_3", "Google Map"}}}};

  const std::vector<Data> updated_data = CreateTestData(data_to_update);
  AddOrUpdate(search_.get(), &task_environment_, updated_data);
  GetSizeAndCheckResults(search_.get(), &task_environment_, 3u);

  Delete(search_.get(), &task_environment_, {"id1"}, 1u);
  Delete(search_.get(), &task_environment_, {"id2", "id3"}, 2u);
  GetSizeAndCheckResults(search_.get(), &task_environment_, 0u);
}

TEST_F(InvertedIndexSearchTest, UpdateDocumentsTest) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1",
       {{"cid_1", "This is a help wi-fi article"},
        {"cid_2", "Another help help wi-fi"}}},
      {"id2", {{"cid_3", "help article on wi-fi"}}}};

  const std::vector<Data> data = CreateTestData(data_to_register);
  AddOrUpdate(search_.get(), &task_environment_, data);
  GetSizeAndCheckResults(search_.get(), &task_environment_, 2u);

  const std::map<std::string, std::vector<ContentWithId>> data_to_update = {
      {"id1",
       {{"cid_1", "This is a help bluetooth article"},
        {"cid_2", "Google Playstore Google Music"}}},
      {"id2", {}},
      {"id3", {{"cid_3", "Google Map"}}}};
  const std::vector<Data> updated_data = CreateTestData(data_to_update);
  UpdateDocuments(search_.get(), &task_environment_, updated_data, 1u);
  GetSizeAndCheckResults(search_.get(), &task_environment_, 2u);

  // Check if "id1" has been updated
  std::vector<Result> results =
      Find(search_.get(), &task_environment_, "bluetooth",
           /*max_results=*/10, ResponseStatus::kSuccess);
  EXPECT_EQ(results.size(), 1u);

  // "bluetooth" only exists in "id1".
  const float expected_score =
      TfIdfScore(/*num_docs=*/2,
                 /*num_docs_with_term=*/1,
                 /*weighted_num_term_occurrence_in_doc=*/1,
                 /*doc_length=*/7);
  CheckResult(results[0], "id1", expected_score,
              /*expected_number_positions=*/1);
}

}  // namespace ash::local_search_service
