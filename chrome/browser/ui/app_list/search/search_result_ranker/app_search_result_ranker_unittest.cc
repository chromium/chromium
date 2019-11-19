// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/app_search_result_ranker.h"

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor.pb.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::UnorderedElementsAre;
using testing::Pair;
using testing::FloatEq;

namespace app_list {

namespace {

constexpr char kTarget1[] = "Target1";
constexpr char kTarget2[] = "Target2";
constexpr bool kNotAnEphemeralUser = false;

}  // namespace

// Test flags of AppSearchResultRanker.
class AppSearchResultRankerFlagTest : public testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();
    // Creates file directory.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  // Waits for all tasks in to finish.
  void Wait() { task_environment_.RunUntilIdle(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppSearchResultRankerFlagTest, TrainAndInfer) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      app_list_features::kEnableZeroStateAppsRanker,
      {{"app_search_result_ranker_predictor_name",
        FakeAppLaunchPredictor::kPredictorName}});

  AppSearchResultRanker ranker(temp_dir_.GetPath(), kNotAnEphemeralUser);
  Wait();
  ranker.Train(kTarget1);
  ranker.Train(kTarget2);
  ranker.Train(kTarget2);

  EXPECT_THAT(ranker.Rank(),
              UnorderedElementsAre(Pair(kTarget1, FloatEq(1.0f)),
                                   Pair(kTarget2, FloatEq(2.0f))));
}

TEST_F(AppSearchResultRankerFlagTest, EphemeralUsersAreDisabled) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      app_list_features::kEnableZeroStateAppsRanker,
      {{"app_search_result_ranker_predictor_name",
        FakeAppLaunchPredictor::kPredictorName}});

  AppSearchResultRanker ranker(temp_dir_.GetPath(), !kNotAnEphemeralUser);
  Wait();
  ranker.Train(kTarget1);
  ranker.Train(kTarget2);
  ranker.Train(kTarget2);

  EXPECT_TRUE(ranker.Rank().empty());
}

TEST_F(AppSearchResultRankerFlagTest, ReturnEmptyIfDisabled) {
  scoped_feature_list_.InitWithFeatures(
      {}, {app_list_features::kEnableZeroStateAppsRanker});

  AppSearchResultRanker ranker(temp_dir_.GetPath(), kNotAnEphemeralUser);
  Wait();
  ranker.Train(kTarget1);
  ranker.Train(kTarget2);

  EXPECT_TRUE(ranker.Rank().empty());
}

// Test Serialization of AppSearchResultRanker.
class AppSearchResultRankerSerializationTest
    : public AppSearchResultRankerFlagTest {
 protected:
  void SetUp() override {
    AppSearchResultRankerFlagTest::SetUp();

    predictor_filename_ =
        temp_dir_.GetPath().AppendASCII("app_launch_predictor");

    // Sets proto.
    (*proto_.mutable_fake_app_launch_predictor()
          ->mutable_rank_result())[kTarget1] = 1.0f;
    (*proto_.mutable_fake_app_launch_predictor()
          ->mutable_rank_result())[kTarget2] = 2.0f;

    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        app_list_features::kEnableZeroStateAppsRanker,
        {{"app_search_result_ranker_predictor_name",
          FakeAppLaunchPredictor::kPredictorName}});
  }

  base::FilePath predictor_filename_;
  AppLaunchPredictorProto proto_;
};

TEST_F(AppSearchResultRankerSerializationTest, LoadFromDiskSucceed) {
  // Prepare file to be loaded.
  const std::string proto_str = proto_.SerializeAsString();
  EXPECT_NE(
      base::WriteFile(predictor_filename_, proto_str.c_str(), proto_str.size()),
      -1);
  // Construct ranker.
  AppSearchResultRanker ranker(temp_dir_.GetPath(), kNotAnEphemeralUser);

  // Check that the file loading is executed in non-blocking way.
  EXPECT_FALSE(ranker.load_from_disk_completed_);

  // Wait for the loading to finish.
  Wait();

  // Check loading is complete.
  EXPECT_TRUE(ranker.load_from_disk_completed_);

  // Check predictor is loaded correctly.
  EXPECT_THAT(ranker.Rank(),
              UnorderedElementsAre(Pair(kTarget1, FloatEq((1.0f))),
                                   Pair(kTarget2, FloatEq(2.0f))));
}

TEST_F(AppSearchResultRankerSerializationTest, LoadFromDiskFailIfNoFileExists) {
  // Construct ranker.
  AppSearchResultRanker ranker(temp_dir_.GetPath(), kNotAnEphemeralUser);
  // Wait for the loading to finish.
  Wait();

  // Check loading is complete.
  EXPECT_TRUE(ranker.load_from_disk_completed_);

  // Check predictor is initialized.
  EXPECT_TRUE(ranker.Rank().empty());
}

TEST_F(AppSearchResultRankerSerializationTest,
       LoadFromDiskFailWithInvalidProto) {
  const std::string wrong_proto = "abc";
  // Prepare file to be loaded.
  EXPECT_NE(base::WriteFile(predictor_filename_, wrong_proto.c_str(),
                            wrong_proto.size()),
            -1);

  // Construct ranker.
  AppSearchResultRanker ranker(temp_dir_.GetPath(), kNotAnEphemeralUser);
  // Wait for the loading to finish.
  Wait();

  // Check loading is complete.
  EXPECT_TRUE(ranker.load_from_disk_completed_);

  // Check predictor is initialized since the proto is not decodable.
  EXPECT_TRUE(ranker.Rank().empty());
}

TEST_F(AppSearchResultRankerSerializationTest, SaveToDiskSucceed) {
  // Construct ranker.
  AppSearchResultRanker ranker(temp_dir_.GetPath(), kNotAnEphemeralUser);
  // Wait for the loading to finish.
  Wait();

  // Check loading is complete.
  EXPECT_TRUE(ranker.load_from_disk_completed_);
  // Check predictor is initialized.
  EXPECT_TRUE(ranker.Rank().empty());

  ranker.Train(kTarget1);
  ranker.Train(kTarget2);

  // Check the predictor file is not created.
  EXPECT_FALSE(base::PathExists(predictor_filename_));

  // Set should_save to true.
  static_cast<FakeAppLaunchPredictor*>(ranker.predictor_.get())
      ->SetShouldSave(true);

  // Train and wait for the writing to finish.
  ranker.Train(kTarget2);
  Wait();

  // Expect the predictor file is created.
  EXPECT_TRUE(base::PathExists(predictor_filename_));

  // Parse the content of the file.
  std::string str_written;
  EXPECT_TRUE(base::ReadFileToString(predictor_filename_, &str_written));
  AppLaunchPredictorProto proto_written;
  EXPECT_TRUE(proto_written.ParseFromString(str_written));

  // Expect the content to be proto_.
  EXPECT_TRUE(EquivToProtoLite(proto_written, proto_));
}

}  // namespace app_list
