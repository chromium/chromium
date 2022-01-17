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

using testing::DoubleEq;
using testing::DoubleNear;
using testing::ElementsAre;

double kEps = 1.0e-5;

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
    params.alpha = 1.0;
    params.gamma = 0.1;
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

  void WriteWeightsToDisk(const std::vector<double>& weights) {
    FtrlOptimizerProto proto;
    proto.set_version(1);
    for (double w : weights)
      proto.add_weights(w);
    WriteToDisk(proto);
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
  auto expert = std::make_unique<FakeExpert>();
  auto* expert_ptr = expert.get();
  experts.push_back(std::move(expert));
  expert_ptr->SetNextScores({0.5});
  FtrlOptimizer ftrl(GetPath(), TestingParams(), std::move(experts));
  ftrl.Score({"abcd"});
  ftrl.Train("abcd");
  Wait();
  ftrl.Score({"abcd"});
  ftrl.Train("abcd");
}

// Given some weights and scores, check we calculate the weighted average
// correctly.
TEST_F(FtrlOptimizerTest, Score) {
  std::vector<std::unique_ptr<FtrlExpert>> experts;
  auto expert_one = std::make_unique<FakeExpert>();
  auto expert_two = std::make_unique<FakeExpert>();
  auto* expert_one_ptr = expert_one.get();
  auto* expert_two_ptr = expert_two.get();
  experts.push_back(std::move(expert_one));
  experts.push_back(std::move(expert_two));

  expert_one_ptr->SetNextScores({0.1, 0.2});
  expert_two_ptr->SetNextScores({0.3, 0.4});

  WriteWeightsToDisk({0.3, 0.7});
  FtrlOptimizer ftrl(GetPath(), TestingParams(), std::move(experts));
  Wait();

  // Expected scores are:
  // 0.1*0.3 + 0.3*0.7 == 0.24
  // 0.2*0.3 + 0.4*0.7 == 0.34
  EXPECT_THAT(ftrl.Score({"a", "b"}),
              ElementsAre(DoubleEq(0.24), DoubleEq(0.34)));
}

// Check that two experts, one that predicts correctly and one incorrectly, get
// the right score adjustments after training.
TEST_F(FtrlOptimizerTest, Train) {
  std::vector<std::unique_ptr<FtrlExpert>> experts;
  auto expert_one = std::make_unique<FakeExpert>();
  auto expert_two = std::make_unique<FakeExpert>();
  auto* expert_one_ptr = expert_one.get();
  auto* expert_two_ptr = expert_two.get();
  experts.push_back(std::move(expert_one));
  experts.push_back(std::move(expert_two));

  expert_one_ptr->SetNextScores({0.2, 0.1});
  expert_two_ptr->SetNextScores({0.1, 0.2});

  WriteWeightsToDisk({0.5, 0.5});
  FtrlOptimizer ftrl(GetPath(), TestingParams(), std::move(experts));
  Wait();

  ftrl.Score({"a", "b"});
  ftrl.Train("a");
  Wait();

  // Expert one predicted {a, b} and expert two predicted {b, a}. The scores
  // pre-normalization should be:
  double one_score = 0.5;
  double two_score = 0.322939;

  double total = one_score + two_score;
  auto proto = ReadFromDisk();
  EXPECT_THAT(proto.weights()[0], DoubleNear(one_score / total, kEps));
  EXPECT_THAT(proto.weights()[1], DoubleNear(two_score / total, kEps));
}

}  // namespace app_list
