// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/local_search_service/inverted_index_search.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "chromeos/components/local_search_service/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace local_search_service {

namespace {

// (content-id, content).
using ContentWithId = std::pair<std::string, std::string>;

// (content-id, content, weight).
using WeightedContentWithId = std::tuple<std::string, std::string, float>;

// (document-id, number-of-occurrences).
using TermOccurrence = std::vector<std::pair<std::string, uint32_t>>;

}  // namespace

class InvertedIndexSearchTest : public testing::Test {
 public:
  void SetUp() override {
    search_ = std::make_unique<InvertedIndexSearch>(IndexId::kCrosSettings,
                                                    nullptr /* local_state */);
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
  search_->AddOrUpdate(data);
  Wait();
  EXPECT_EQ(search_->GetSize(), 2u);

  {
    // "network" does not exist in the index.
    const TermOccurrence doc_with_freq =
        search_->FindTermForTesting(base::UTF8ToUTF16("network"));
    EXPECT_TRUE(doc_with_freq.empty());
  }

  {
    // "help" exists in the index.
    const TermOccurrence doc_with_freq =
        search_->FindTermForTesting(base::UTF8ToUTF16("help"));
    EXPECT_EQ(doc_with_freq.size(), 2u);
    EXPECT_EQ(doc_with_freq[0].first, "id1");
    EXPECT_EQ(doc_with_freq[0].second, 3u);
    EXPECT_EQ(doc_with_freq[1].first, "id2");
    EXPECT_EQ(doc_with_freq[1].second, 1u);
  }

  {
    // "wifi" exists in the index but "wi-fi" doesn't because of normalization.
    TermOccurrence doc_with_freq =
        search_->FindTermForTesting(base::UTF8ToUTF16("wifi"));
    EXPECT_EQ(doc_with_freq.size(), 2u);
    EXPECT_EQ(doc_with_freq[0].first, "id1");
    EXPECT_EQ(doc_with_freq[0].second, 2u);
    EXPECT_EQ(doc_with_freq[1].first, "id2");
    EXPECT_EQ(doc_with_freq[1].second, 1u);

    doc_with_freq = search_->FindTermForTesting(base::UTF8ToUTF16("wi-fi"));
    EXPECT_TRUE(doc_with_freq.empty());

    // "WiFi" doesn't exist because the index stores normalized word.
    doc_with_freq = search_->FindTermForTesting(base::UTF8ToUTF16("WiFi"));
    EXPECT_TRUE(doc_with_freq.empty());
  }

  {
    // "this" does not exist in the index because it's a stopword
    const TermOccurrence doc_with_freq =
        search_->FindTermForTesting(base::UTF8ToUTF16("this"));
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
  search_->AddOrUpdate(data);
  Wait();
  EXPECT_EQ(search_->GetSize(), 2u);

  const std::map<std::string, std::vector<ContentWithId>> data_to_update = {
      {"id1",
       {{"cid_1", "This is a help bluetooth article"},
        {"cid_2", "Google Playstore Google Music"}}},
      {"id3", {{"cid_3", "Google Map"}}}};

  const std::vector<Data> updated_data = CreateTestData(data_to_update);
  search_->AddOrUpdate(updated_data);
  Wait();
  EXPECT_EQ(search_->GetSize(), 3u);

  {
    const TermOccurrence doc_with_freq =
        search_->FindTermForTesting(base::UTF8ToUTF16("bluetooth"));
    EXPECT_EQ(doc_with_freq.size(), 1u);
    EXPECT_EQ(doc_with_freq[0].first, "id1");
    EXPECT_EQ(doc_with_freq[0].second, 1u);
  }

  {
    const TermOccurrence doc_with_freq =
        search_->FindTermForTesting(base::UTF8ToUTF16("wifi"));
    EXPECT_EQ(doc_with_freq.size(), 1u);
    EXPECT_EQ(doc_with_freq[0].first, "id2");
    EXPECT_EQ(doc_with_freq[0].second, 1u);
  }

  {
    const TermOccurrence doc_with_freq =
        search_->FindTermForTesting(base::UTF8ToUTF16("google"));
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
  search_->AddOrUpdate(data);
  Wait();
  EXPECT_EQ(search_->GetSize(), 2u);

  EXPECT_EQ(search_->Delete({"id1", "id3"}), 1u);
  Wait();

  {
    const TermOccurrence doc_with_freq =
        search_->FindTermForTesting(base::UTF8ToUTF16("wifi"));
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

  search_->AddOrUpdate(data);
  Wait();
  EXPECT_EQ(search_->GetSize(), 2u);

  search_->ClearIndex();
  Wait();
  EXPECT_EQ(search_->GetSize(), 0u);
}

TEST_F(InvertedIndexSearchTest, Find) {
  const std::map<std::string, std::vector<WeightedContentWithId>>
      data_to_register = {{"id1",
                           {{"cid_1", "This is a help wi-fi article", 0.8},
                            {"cid_2", "Another help help wi-fi", 0.6}}},
                          {"id2", {{"cid_3", "help article on wi-fi", 0.6}}}};
  const std::vector<Data> data = CreateTestData(data_to_register);

  // Nothing has been added to the index.
  std::vector<Result> results;
  EXPECT_EQ(
      search_->Find(base::UTF8ToUTF16("network"), /*max_results=*/10, &results),
      ResponseStatus::kEmptyIndex);
  EXPECT_TRUE(results.empty());

  // Data is added and then deleted from index, making the index empty.
  search_->AddOrUpdate(data);
  Wait();
  EXPECT_EQ(search_->GetSize(), 2u);
  EXPECT_EQ(search_->Delete({"id1", "id2"}), 2u);
  Wait();
  EXPECT_EQ(search_->GetSize(), 0u);

  EXPECT_EQ(
      search_->Find(base::UTF8ToUTF16("network"), /*max_results=*/10, &results),
      ResponseStatus::kEmptyIndex);
  EXPECT_TRUE(results.empty());

  // Index is populated again, but query is empty.
  search_->AddOrUpdate(data);
  Wait();
  EXPECT_EQ(search_->GetSize(), 2u);

  EXPECT_EQ(search_->Find(base::UTF8ToUTF16(""), /*max_results=*/10, &results),
            ResponseStatus::kEmptyQuery);
  EXPECT_TRUE(results.empty());

  // No document is found for a given query.
  EXPECT_EQ(search_->Find(base::UTF8ToUTF16("networkstuff"), /*max_results=*/10,
                          &results),
            ResponseStatus::kSuccess);
  EXPECT_TRUE(results.empty());

  {
    // A document is found.
    // Query's case is normalized.
    EXPECT_EQ(search_->Find(base::UTF8ToUTF16("ANOTHER networkstuff"),
                            /*max_results=*/10, &results),
              ResponseStatus::kSuccess);
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
    EXPECT_EQ(search_->Find(base::UTF8ToUTF16("another help"),
                            /*max_results=*/10, &results),
              ResponseStatus::kSuccess);
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
    EXPECT_EQ(search_->Find(base::UTF8ToUTF16("another help"),
                            /*max_results=*/1, &results),
              ResponseStatus::kSuccess);
    EXPECT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].id, "id1");
  }

  {
    // Same as above, but set max_results to 0, meaning no max.
    EXPECT_EQ(search_->Find(base::UTF8ToUTF16("another help"),
                            /*max_results=*/0, &results),
              ResponseStatus::kSuccess);
    EXPECT_EQ(results.size(), 2u);
  }
}

}  // namespace local_search_service
}  // namespace chromeos
