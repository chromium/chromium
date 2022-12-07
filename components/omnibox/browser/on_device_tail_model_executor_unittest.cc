// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "components/omnibox/browser/on_device_tail_model_executor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAreArray;

namespace {

const int kNumLayer = 1;
const int kStateSize = 512;
const int kEmbeddingDim = 64;

base::FilePath GetTestFilePath(const std::string& filename) {
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
  std::string fullname = "components/test/data/omnibox/" + filename;
  file_path = file_path.AppendASCII(fullname);
  return file_path;
}

}  // namespace

class OnDeviceTailModelExecutorTest : public ::testing::Test {
 public:
  OnDeviceTailModelExecutorTest() {
    executor_ = std::make_unique<OnDeviceTailModelExecutor>();
    EXPECT_TRUE(executor_->Init(GetTestFilePath("test_tail_model.tflite"),
                                GetTestFilePath("vocab_test.txt"), kStateSize,
                                kNumLayer, kEmbeddingDim));
  }

 protected:
  void TearDown() override { executor_.reset(); }

  std::vector<float> GetPrevQueryCache(
      const OnDeviceTailTokenizer::TokenIds& token_ids) {
    std::vector<float> result;
    auto iter = executor_->prev_query_cache_.Get(token_ids);
    if (iter != executor_->prev_query_cache_.end()) {
      result = iter->second;
    }
    return result;
  }

  std::unique_ptr<OnDeviceTailModelExecutor> executor_;
};

TEST_F(OnDeviceTailModelExecutorTest, TestEncodePreviousQuery) {
  OnDeviceTailTokenizer::TokenIds ids1({16}), ids2({16, 17});
  std::vector<float> encoding1, encoding2, cached_encoding;

  EXPECT_TRUE(executor_->EncodePreviousQuery(ids1, &encoding1));
  EXPECT_TRUE(executor_->EncodePreviousQuery(ids2, &encoding2));
  EXPECT_TRUE(executor_->EncodePreviousQuery(ids1, &encoding1));

  EXPECT_NE(encoding1, encoding2);
  EXPECT_EQ(GetPrevQueryCache(ids1), encoding1);
  EXPECT_EQ(GetPrevQueryCache(ids2), encoding2);
}
