// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/core/search_strings_update_listener.h"

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_embeddings {

namespace {

base::FilePath GetTestFilePath(const std::string& file_name) {
  base::FilePath test_data_dir;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir);
  return test_data_dir.AppendASCII("components/test/data/history_embeddings")
      .AppendASCII(file_name);
}

}  // namespace

class SearchStringsUpdateListenerTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(listener()->filter_words_hashes().empty());
  }

  void VerifyFilterWordsHashes(
      std::unordered_set<uint32_t> filter_words_hashes) {
    task_environment()->RunUntilIdle();
    ASSERT_EQ(listener()->filter_words_hashes(), filter_words_hashes);
  }

  void VerifyStopWordsHashes(std::unordered_set<uint32_t> stop_words_hashes) {
    task_environment()->RunUntilIdle();
    ASSERT_EQ(listener()->stop_words_hashes(), stop_words_hashes);
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  SearchStringsUpdateListener* listener() {
    return SearchStringsUpdateListener::GetInstance();
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

// Tests that filter hashes and stop word hashes are loaded from disk and are
// not reset by bad, empty, or nonexisting files.
TEST_F(SearchStringsUpdateListenerTest, UpdateHashes) {
  listener()->OnSearchStringsUpdate(
      GetTestFilePath("fake_search_strings_file"));

  // Hashes for "special", "something something", "hello world".
  VerifyFilterWordsHashes({3962775614, 4220142007, 430397466});

  // Hashes for "the", "and".
  VerifyStopWordsHashes({2374167618, 754760635});

  listener()->OnSearchStringsUpdate(GetTestFilePath("bad_search_strings_file"));
  VerifyFilterWordsHashes({3962775614, 4220142007, 430397466});
  VerifyStopWordsHashes({2374167618, 754760635});

  listener()->OnSearchStringsUpdate(
      GetTestFilePath("emtpy_search_strings_file"));
  VerifyFilterWordsHashes({3962775614, 4220142007, 430397466});
  VerifyStopWordsHashes({2374167618, 754760635});

  listener()->OnSearchStringsUpdate(
      GetTestFilePath("foobar_search_strings_file"));
  VerifyFilterWordsHashes({3962775614, 4220142007, 430397466});
  VerifyStopWordsHashes({2374167618, 754760635});
}

}  // namespace history_embeddings
