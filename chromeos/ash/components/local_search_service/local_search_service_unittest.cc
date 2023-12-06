// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/local_search_service/local_search_service.h"
#include <memory>

#include "base/test/task_environment.h"
#include "chromeos/ash/components/local_search_service/search_metrics_reporter.h"
#include "chromeos/ash/components/local_search_service/test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::local_search_service {

namespace {

// (content-id, content).
using ContentWithId = std::pair<std::string, std::string>;

}  // namespace

class LocalSearchServiceTest : public testing::Test {
 public:
  void SetUp() override {
    SearchMetricsReporter::RegisterLocalStatePrefs(pref_service_.registry());
    reporter_ = std::make_unique<SearchMetricsReporter>(&pref_service_);

    lss_service_ = std::make_unique<LocalSearchService>(
        lss_service_remote_.BindNewPipeAndPassReceiver());
  }

  void BindIndexAndCheck(
      IndexId index_id,
      Backend backend,
      mojo::PendingReceiver<mojom::Index> index_receiver,
      mojo::PendingRemote<mojom::SearchMetricsReporter> reporter_remote) {
    bool callback_done = false;
    std::string error = "";
    lss_service_remote_->BindIndex(
        index_id, backend, std::move(index_receiver),
        std::move(reporter_remote),
        base::BindOnce(
            [](bool* callback_done, std::string* error,
               const std::optional<std::string>& error_callback) {
              *callback_done = true;
              if (error_callback)
                *error = error_callback.value();
            },
            &callback_done, &error));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(callback_done);
    EXPECT_EQ(error, "");
  }

  void IndexAddOrUpdate(mojo::Remote<mojom::Index>* index_remote,
                        const std::vector<Data>& data) {
    bool callback_done = false;
    (*index_remote)
        ->AddOrUpdate(
            data,
            base::BindOnce([](bool* callback_done) { *callback_done = true; },
                           &callback_done));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(callback_done);
  }

  void IndexGetSizeAndCheckResults(mojo::Remote<mojom::Index>* index_remote,
                                   uint32_t expected_num_items) {
    bool callback_done = false;
    uint32_t num_items = 0;
    (*index_remote)
        ->GetSize(base::BindOnce(
            [](bool* callback_done, uint32_t* num_items, uint64_t size) {
              *callback_done = true;
              *num_items = size;
            },
            &callback_done, &num_items));
    task_environment_.RunUntilIdle();
    ASSERT_TRUE(callback_done);
    EXPECT_EQ(num_items, expected_num_items);
  }

 protected:
  mojo::Remote<mojom::LocalSearchService> lss_service_remote_;
  std::unique_ptr<LocalSearchService> lss_service_;
  std::unique_ptr<SearchMetricsReporter> reporter_;
  TestingPrefServiceSimple pref_service_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
};

TEST_F(LocalSearchServiceTest, BindIndexSuccessfuly) {
  mojo::Remote<mojom::Index> index_remote;
  BindIndexAndCheck(IndexId::kCrosSettings, Backend::kLinearMap,
                    index_remote.BindNewPipeAndPassReceiver(),
                    reporter_->BindNewPipeAndPassRemote());
  IndexGetSizeAndCheckResults(&index_remote, 0u);
}

TEST_F(LocalSearchServiceTest, UseLinearMap) {
  mojo::Remote<mojom::Index> index_remote;
  BindIndexAndCheck(IndexId::kCrosSettings, Backend::kLinearMap,
                    index_remote.BindNewPipeAndPassReceiver(),
                    reporter_->BindNewPipeAndPassRemote());
  IndexGetSizeAndCheckResults(&index_remote, 0u);

  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1",
       {{"cid_1", "This is a help wi-fi article"},
        {"cid_2", "Another help help wi-fi"}}},
      {"id2", {{"cid_3", "help article on wi-fi"}}}};
  const std::vector<Data> data = CreateTestData(data_to_register);
  IndexAddOrUpdate(&index_remote, data);
  IndexGetSizeAndCheckResults(&index_remote, 2u);
}

TEST_F(LocalSearchServiceTest, UseInvertedIndex) {
  mojo::Remote<mojom::Index> index_remote;
  BindIndexAndCheck(IndexId::kCrosSettings, Backend::kInvertedIndex,
                    index_remote.BindNewPipeAndPassReceiver(),
                    mojo::NullRemote());
  IndexGetSizeAndCheckResults(&index_remote, 0u);

  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1",
       {{"cid_1", "This is a help wi-fi article"},
        {"cid_2", "Another help help wi-fi"}}},
      {"id2", {{"cid_3", "help article on wi-fi"}}}};
  const std::vector<Data> data = CreateTestData(data_to_register);
  IndexAddOrUpdate(&index_remote, data);
  IndexGetSizeAndCheckResults(&index_remote, 2u);
}

TEST_F(LocalSearchServiceTest, BindMultipleTimes) {
  mojo::Remote<mojom::Index> first_index_remote;
  BindIndexAndCheck(IndexId::kCrosSettings, Backend::kInvertedIndex,
                    first_index_remote.BindNewPipeAndPassReceiver(),
                    reporter_->BindNewPipeAndPassRemote());
  IndexGetSizeAndCheckResults(&first_index_remote, 0u);
  const std::map<std::string, std::vector<ContentWithId>> data_to_register = {
      {"id1",
       {{"cid_1", "This is a help wi-fi article"},
        {"cid_2", "Another help help wi-fi"}}},
      {"id2", {{"cid_3", "help article on wi-fi"}}}};
  const std::vector<Data> data = CreateTestData(data_to_register);
  IndexAddOrUpdate(&first_index_remote, data);
  IndexGetSizeAndCheckResults(&first_index_remote, 2u);

  // Bind second time, the index should still be the same.
  mojo::Remote<mojom::Index> second_index_remote;
  BindIndexAndCheck(IndexId::kCrosSettings, Backend::kInvertedIndex,
                    second_index_remote.BindNewPipeAndPassReceiver(),
                    reporter_->BindNewPipeAndPassRemote());
  IndexGetSizeAndCheckResults(&second_index_remote, 2u);

  // Bind the third time with different id, should get a new index.
  mojo::Remote<mojom::Index> third_index_remote;
  BindIndexAndCheck(IndexId::kHelpApp, Backend::kInvertedIndex,
                    third_index_remote.BindNewPipeAndPassReceiver(),
                    reporter_->BindNewPipeAndPassRemote());
  IndexGetSizeAndCheckResults(&third_index_remote, 0u);
}

}  // namespace ash::local_search_service
