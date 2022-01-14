// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/util/ftrl_optimizer.h"

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {
namespace {

class FakeExpert : public FtrlExpert {
 public:
  void SetNextScores(const std::vector<double>& scores) {
    next_scores_ = scores;
  }

  ~FakeExpert() override = default;

  std::vector<double> Score(const std::vector<std::string>& items) override {
    return next_scores_;
  }

  void Train(const std::string& item) override {}

 private:
  std::vector<double> next_scores_;
};

}  // namespace

class FtrlOptimizerTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() { return temp_dir_.GetPath().Append("proto"); }

  FtrlOptimizer::Params TestingParams() {
    FtrlOptimizer::Params params;
    return params;
  }

  void ClearDisk() {
    base::DeleteFile(GetPath());
    ASSERT_FALSE(base::PathExists(GetPath()));
  }

  FtrlOptimizerProto ReadFromDisk() {
    std::string proto_str;
    CHECK(base::ReadFileToString(GetPath(), &proto_str));
    FtrlOptimizerProto proto;
    CHECK(proto.ParseFromString(proto_str));
    return proto;
  }

  void WriteToDisk(const FtrlOptimizerProto& proto) {
    ASSERT_TRUE(base::WriteFile(GetPath(), proto.SerializeAsString()));
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::UI,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::ScopedTempDir temp_dir_;
};

// Instantiate and call some methods.
TEST_F(FtrlOptimizerTest, SmokeTest) {
  std::vector<std::unique_ptr<FtrlExpert>> experts;
  experts.push_back(std::make_unique<FakeExpert>());
  FtrlOptimizer ftrl(GetPath(), TestingParams(), std::move(experts));
  ftrl.Score({"abcd"});
  ftrl.Train("abcd");
}

}  // namespace app_list
