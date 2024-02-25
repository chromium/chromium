// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/local_search_service/linear_map_search.h"
#include "chromeos/ash/components/local_search_service/shared_structs.h"
#include "chromeos/ash/components/local_search_service/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::local_search_service {

namespace {

// This is (data-id, content-ids).
using ResultWithIds = std::pair<std::string, std::vector<std::string>>;
using ContentWithId = std::pair<std::string, std::string>;

void CheckSearchParams(const SearchParams& actual,
                       const SearchParams& expected) {
  EXPECT_DOUBLE_EQ(actual.relevance_threshold, expected.relevance_threshold);
  EXPECT_DOUBLE_EQ(actual.prefix_threshold, expected.prefix_threshold);
  EXPECT_DOUBLE_EQ(actual.fuzzy_threshold, expected.fuzzy_threshold);
}

void GetSizeAndCheckResults(LinearMapSearch* index,
                            uint32_t expectd_num_items) {
  bool callback_done = false;
  uint32_t num_items = 0;
  index->GetSize(base::BindOnce(
      [](bool* callback_done, uint32_t* num_items, uint64_t size) {
        *callback_done = true;
        *num_items = size;
      },
      &callback_done, &num_items));
  ASSERT_TRUE(callback_done);
  EXPECT_EQ(num_items, expectd_num_items);
}

void AddOrUpdate(LinearMapSearch* index, const std::vector<Data>& data) {
  bool callback_done = false;
  index->AddOrUpdate(
      data, base::BindOnce([](bool* callback_done) { *callback_done = true; },
                           &callback_done));
  ASSERT_TRUE(callback_done);
}

void UpdateDocumentsAndCheckResults(LinearMapSearch* index,
                                    const std::vector<Data>& data,
                                    uint32_t expect_num_deleted) {
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
  ASSERT_TRUE(callback_done);
  EXPECT_EQ(num_deleted, expect_num_deleted);
}

void FindAndCheckResults(LinearMapSearch* index,
                         std::string query,
                         int32_t max_results,
                         ResponseStatus expected_status,
                         const std::vector<ResultWithIds>& expected_results) {
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

  ASSERT_TRUE(callback_done);
  EXPECT_EQ(status, expected_status);

  if (!results.empty()) {
    // If results are returned, check size and values match the expected.
    EXPECT_EQ(results.size(), expected_results.size());
    for (size_t i = 0; i < results.size(); ++i) {
      EXPECT_EQ(results[i].id, expected_results[i].first);
      EXPECT_EQ(results[i].positions.size(), expected_results[i].second.size());

      for (size_t j = 0; j < results[i].positions.size(); ++j) {
        EXPECT_EQ(results[i].positions[j].content_id,
                  expected_results[i].second[j]);
      }
      // Scores should be non-increasing.
      if (i < results.size() - 1) {
        EXPECT_GE(results[i].score, results[i + 1].score);
      }
    }
    return;
  }

  // If no results are returned, expected ids should be empty.
  EXPECT_TRUE(expected_results.empty());
}

}  // namespace

class LinearMapSearchTest : public testing::Test {
  void SetUp() override {
    index_ = std::make_unique<LinearMapSearch>(IndexId::kCrosSettings);
  }

 protected:
  std::unique_ptr<LinearMapSearch> index_;
};

TEST_F(LinearMapSearchTest, SetSearchParams) {
  {
    // No params are specified so default values are used.
    const SearchParams used_params = index_->GetSearchParamsForTesting();

    CheckSearchParams(used_params, SearchParams());
  }

  {
    // Params are specified and are used.
    SearchParams search_params;
    const SearchParams default_params;
    search_params.relevance_threshold = default_params.relevance_threshold / 2;
    search_params.prefix_threshold = default_params.prefix_threshold / 2;
    search_params.fuzzy_threshold = default_params.fuzzy_threshold / 2;

    index_->SetSearchParams(search_params);

    const SearchParams used_params = index_->GetSearchParamsForTesting();

    CheckSearchParams(used_params, search_params);
  }
}

TEST_F(LinearMapSearchTest, RelevanceThreshold) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1", {{"tag1", "Wi-Fi"}}}, {"id2", {{"tag2", "famous"}}}};
  std::vector<Data> data = CreateTestData(data_to_register);

  AddOrUpdate(index_.get(), data);
  GetSizeAndCheckResults(index_.get(), 2u);

  {
    SearchParams search_params;
    search_params.relevance_threshold = 0.0;
    index_->SetSearchParams(search_params);

    const std::vector<ResultWithIds> expected_results = {{"id1", {"tag1"}},
                                                         {"id2", {"tag2"}}};
    FindAndCheckResults(index_.get(), "wifi",
                        /*max_results=*/-1, ResponseStatus::kSuccess,
                        expected_results);
  }
  {
    SearchParams search_params;
    search_params.relevance_threshold = 0.3;
    index_->SetSearchParams(search_params);

    const std::vector<ResultWithIds> expected_results = {{"id1", {"tag1"}}};
    FindAndCheckResults(index_.get(), "wifi",
                        /*max_results=*/-1, ResponseStatus::kSuccess,
                        expected_results);
  }
  {
    SearchParams search_params;
    search_params.relevance_threshold = 0.95;
    index_->SetSearchParams(search_params);

    FindAndCheckResults(index_.get(), "wifi",
                        /*max_results=*/-1, ResponseStatus::kSuccess, {});
  }
}

TEST_F(LinearMapSearchTest, MaxResults) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1", {{"tag1", "abcde"}, {"tag2", "Wi-Fi"}}},
      {"id2", {{"tag3", "wifi"}}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  AddOrUpdate(index_.get(), data);
  GetSizeAndCheckResults(index_.get(), 2u);

  SearchParams search_params;
  search_params.relevance_threshold = 0.3;
  index_->SetSearchParams(search_params);

  {
    const std::vector<ResultWithIds> expected_results = {{"id2", {"tag3"}},
                                                         {"id1", {"tag2"}}};
    FindAndCheckResults(index_.get(), "wifi",
                        /*max_results=*/-1, ResponseStatus::kSuccess,
                        expected_results);
  }
  {
    const std::vector<ResultWithIds> expected_results = {{"id2", {"tag3"}}};
    FindAndCheckResults(index_.get(), "wifi",
                        /*max_results=*/1, ResponseStatus::kSuccess,
                        expected_results);
  }
}

TEST_F(LinearMapSearchTest, ResultFound) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1", {{"cid1", "id1"}, {"cid2", "tag1a"}, {"cid3", "tag1b"}}},
      {"xyz", {{"cid4", "xyz"}}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  EXPECT_EQ(data.size(), 2u);

  AddOrUpdate(index_.get(), data);
  GetSizeAndCheckResults(index_.get(), 2u);

  // Find result with query "id1". It returns an exact match.
  const std::vector<ResultWithIds> expected_results = {{"id1", {"cid1"}}};
  FindAndCheckResults(index_.get(), "id1",
                      /*max_results=*/-1, ResponseStatus::kSuccess,
                      expected_results);
  FindAndCheckResults(index_.get(), "abc",
                      /*max_results=*/-1, ResponseStatus::kSuccess, {});
}

TEST_F(LinearMapSearchTest, ClearIndex) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1", {{"cid1", "id1"}, {"cid2", "tag1a"}, {"cid3", "tag1b"}}},
      {"xyz", {{"cid4", "xyz"}}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  EXPECT_EQ(data.size(), 2u);

  AddOrUpdate(index_.get(), data);
  GetSizeAndCheckResults(index_.get(), 2u);

  bool callback_done = false;
  index_->ClearIndex(base::BindOnce(
      [](bool* callback_done) { *callback_done = true; }, &callback_done));
  ASSERT_TRUE(callback_done);
  GetSizeAndCheckResults(index_.get(), 0u);
}

TEST_F(LinearMapSearchTest, UpdateDocuments) {
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1", {{"cid1", "id1"}, {"cid2", "tag1a"}, {"cid3", "tag1b"}}},
      {"xyz", {{"cid4", "xyz"}}}};
  std::vector<Data> data = CreateTestData(data_to_register);
  EXPECT_EQ(data.size(), 2u);

  AddOrUpdate(index_.get(), data);
  GetSizeAndCheckResults(index_.get(), 2u);

  const std::map<std::string, std::vector<ContentWithId>>
      update_data_to_register = {{"id1",
                                  {{"update cid1", "update id1"},
                                   {"update cid2", "update tag1a"},
                                   {"update cid3", "update tag1b"}}},
                                 {"xyz", {}},
                                 {"nonexistid", {}}};
  std::vector<Data> update_data = CreateTestData(update_data_to_register);
  EXPECT_EQ(update_data.size(), 3u);

  UpdateDocumentsAndCheckResults(index_.get(), update_data, 1u);
  GetSizeAndCheckResults(index_.get(), 1u);
}

}  // namespace ash::local_search_service
