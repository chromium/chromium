// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker.h"

#include <vector>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash/hash.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/app_launch_predictor_test_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/frecency_store.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/histogram_util.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_predictor.h"
#include "chrome/browser/ui/app_list/search/search_result_ranker/recurrence_ranker_config.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::ElementsAre;
using testing::FloatEq;
using testing::Pair;
using testing::StrEq;
using testing::UnorderedElementsAre;

namespace app_list {

namespace {

// For convenience, sets all fields of a config proto except for the predictor.
void PartiallyPopulateConfig(RecurrenceRankerConfigProto* config) {
  config->set_target_limit(100u);
  config->set_target_decay(0.8f);
  config->set_condition_limit(101u);
  config->set_condition_decay(0.81f);
  config->set_min_seconds_between_saves(5);
}

}  // namespace

class RecurrenceRankerTest : public testing::Test {
 protected:
  void SetUp() override {
    Test::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    ranker_filepath_ = temp_dir_.GetPath().AppendASCII("recurrence_ranker");
    Wait();
  }

  void Wait() { task_environment_.RunUntilIdle(); }

  // Returns the config for a ranker with a fake predictor.
  RecurrenceRankerConfigProto MakeSimpleConfig() {
    RecurrenceRankerConfigProto config;
    PartiallyPopulateConfig(&config);
    // Even if empty, the setting of the oneof |predictor_config| in
    // RecurrenceRankerConfigProto is used to determine which predictor is
    // constructed.
    config.mutable_predictor()->mutable_fake_predictor();
    return config;
  }

  // Returns a ranker using a fake predictor.
  std::unique_ptr<RecurrenceRanker> MakeSimpleRanker() {
    auto ranker = std::make_unique<RecurrenceRanker>(
        "MyModel", ranker_filepath_, MakeSimpleConfig(), false);
    // There should be no model file written to disk immediately after
    // construction, but there should be one once initialization is complete.
    EXPECT_FALSE(base::PathExists(ranker_filepath_));
    Wait();
    EXPECT_TRUE(base::PathExists(ranker_filepath_));
    return ranker;
  }

  RecurrenceRankerProto MakeTestingProto() {
    RecurrenceRankerProto proto;
    proto.set_config_hash(
        base::PersistentHash(MakeSimpleConfig().SerializeAsString()));

    // Make target frecency store.
    auto* targets = proto.mutable_targets();
    targets->set_value_limit(100u);
    targets->set_decay_coeff(0.8f);
    targets->set_num_updates(4);
    targets->set_next_id(3);
    auto* target_values = targets->mutable_values();

    FrecencyStoreProto::ValueData value_data;
    value_data.set_id(0u);
    value_data.set_last_score(0.5f);
    value_data.set_last_num_updates(1);
    (*target_values)["A"] = value_data;

    value_data = FrecencyStoreProto::ValueData();
    value_data.set_id(1u);
    value_data.set_last_score(0.5f);
    value_data.set_last_num_updates(3);
    (*target_values)["B"] = value_data;

    value_data = FrecencyStoreProto::ValueData();
    value_data.set_id(2u);
    value_data.set_last_score(0.5f);
    value_data.set_last_num_updates(4);
    (*target_values)["C"] = value_data;

    // Make conditions frecency store.
    auto* conditions = proto.mutable_conditions();
    conditions->set_value_limit(10u);
    conditions->set_decay_coeff(0.5f);
    conditions->set_num_updates(0);
    conditions->set_next_id(0);

    auto* condition_values = conditions->mutable_values();

    value_data.set_id(0u);
    value_data.set_last_score(0.5f);
    value_data.set_last_num_updates(1);
    (*condition_values)[""] = value_data;

    // Make FakePredictor counts.
    auto* counts =
        proto.mutable_predictor()->mutable_fake_predictor()->mutable_counts();
    (*counts)[0u] = 1.0f;
    (*counts)[1u] = 2.0f;
    (*counts)[2u] = 1.0f;

    return proto;
  }

  void ExpectErrors(bool fresh_model_created = true,
                    bool using_fake_predictor = true,
                    bool has_saved = false) {
    // Total count of serialization reports:
    //  - one for either a kLoadOk or kModelReadError
    //  - one if |fresh_model_created| because model is written to disk with a
    //    kSaveOk on initialization.
    //  - one if |has_saved| because model is again written to disk with a
    //    kSaveOk.
    histogram_tester_.ExpectTotalCount(
        "RecurrenceRanker.SerializationStatus.MyModel",
        1 + static_cast<int>(has_saved) +
            static_cast<int>(fresh_model_created));

    // If a model doesn't already exist, a read error is logged.
    if (fresh_model_created) {
      histogram_tester_.ExpectBucketCount(
          "RecurrenceRanker.SerializationStatus.MyModel",
          SerializationStatus::kModelReadError, 1);
    } else {
      histogram_tester_.ExpectBucketCount(
          "RecurrenceRanker.SerializationStatus.MyModel",
          SerializationStatus::kLoadOk, 1);
    }

    histogram_tester_.ExpectBucketCount(
        "RecurrenceRanker.SerializationStatus.MyModel",
        SerializationStatus::kSaveOk,
        static_cast<int>(has_saved) + static_cast<int>(fresh_model_created));

    // Initialising with the fake predictor logs an UMA error, because it should
    // be used only in tests and not in production.
    if (using_fake_predictor) {
      histogram_tester_.ExpectTotalCount(
          "RecurrenceRanker.InitializationStatus.MyModel", 2);
      histogram_tester_.ExpectBucketCount(
          "RecurrenceRanker.InitializationStatus.MyModel",
          InitializationStatus::kFakePredictorUsed, 1);
      histogram_tester_.ExpectBucketCount(
          "RecurrenceRanker.InitializationStatus.MyModel",
          InitializationStatus::kInitialized, 1);
    } else {
      histogram_tester_.ExpectUniqueSample(
          "RecurrenceRanker.InitializationStatus.MyModel",
          InitializationStatus::kInitialized, 1);
    }
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED};
  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;

  base::FilePath ranker_filepath_;
};

TEST_F(RecurrenceRankerTest, Record) {
  auto ranker = MakeSimpleRanker();

  ranker->Record("A");
  ranker->Record("B");
  ranker->Record("B");

  EXPECT_THAT(ranker->Rank(), UnorderedElementsAre(Pair("A", FloatEq(1.0f)),
                                                   Pair("B", FloatEq(2.0f))));
  ExpectErrors(/* fresh_model_created = */ true,
               /* using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, RenameTarget) {
  auto ranker = MakeSimpleRanker();

  ranker->Record("A");
  ranker->Record("B");
  ranker->Record("B");
  ranker->RenameTarget("B", "A");

  EXPECT_THAT(ranker->Rank(), ElementsAre(Pair("A", FloatEq(2.0f))));
  ExpectErrors(/* fresh_model_created = */ true,
               /* using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, RemoveTarget) {
  auto ranker = MakeSimpleRanker();

  ranker->Record("A");
  ranker->Record("B");
  ranker->Record("B");
  ranker->RemoveTarget("A");

  EXPECT_THAT(ranker->Rank(), ElementsAre(Pair("B", FloatEq(2.0f))));
  ExpectErrors(/* fresh_model_created = */ true,
               /* using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, ComplexRecordAndRank) {
  auto ranker = MakeSimpleRanker();

  ranker->Record("A");
  ranker->Record("B");
  ranker->Record("C");
  ranker->Record("B");
  ranker->RenameTarget("D", "C");
  ranker->RemoveTarget("F");
  ranker->RenameTarget("C", "F");
  ranker->RemoveTarget("A");
  ranker->RenameTarget("C", "F");
  ranker->Record("A");

  EXPECT_THAT(ranker->Rank(), UnorderedElementsAre(Pair("A", FloatEq(1.0f)),
                                                   Pair("B", FloatEq(2.0f)),
                                                   Pair("F", FloatEq(1.0f))));
  ExpectErrors(/* fresh_model_created = */ true,
               /* using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, RankTopN) {
  auto ranker = MakeSimpleRanker();

  const std::vector<std::string> targets = {"B", "A", "A", "B", "C",
                                            "B", "D", "C", "A", "A"};
  for (auto target : targets)
    ranker->Record(target);

  EXPECT_THAT(ranker->RankTopN(2),
              ElementsAre(Pair("A", FloatEq(4.0f)), Pair("B", FloatEq(3.0f))));
  EXPECT_THAT(ranker->RankTopN(100),
              ElementsAre(Pair("A", FloatEq(4.0f)), Pair("B", FloatEq(3.0f)),
                          Pair("C", FloatEq(2.0f)), Pair("D", FloatEq(1.0f))));
  ExpectErrors(/* fresh_model_created = */ true,
               /* using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, LoadFromDisk) {
  // Serialise a testing proto.
  RecurrenceRankerProto proto = MakeTestingProto();
  const std::string proto_str = proto.SerializeAsString();
  EXPECT_NE(
      base::WriteFile(ranker_filepath_, proto_str.c_str(), proto_str.size()),
      -1);

  // Make a ranker.
  RecurrenceRanker ranker("MyModel", ranker_filepath_, MakeSimpleConfig(),
                          false);

  // Check that the file loading is executed in non-blocking way.
  EXPECT_FALSE(ranker.load_from_disk_completed_);
  Wait();
  EXPECT_TRUE(ranker.load_from_disk_completed_);

  // Check predictor is loaded correctly.
  EXPECT_THAT(ranker.Rank(), UnorderedElementsAre(Pair("A", FloatEq(1.0f)),
                                                  Pair("B", FloatEq(2.0f)),
                                                  Pair("C", FloatEq(1.0f))));
  ExpectErrors(/* fresh_model_created = */ false,
               /* using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, InitializeIfNoFileExists) {
  // Set up a temp dir with no saved ranker.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath filepath =
      temp_dir.GetPath().AppendASCII("recurrence_ranker_invalid");

  RecurrenceRanker ranker("MyModel", filepath, MakeSimpleConfig(), false);
  Wait();

  EXPECT_TRUE(ranker.load_from_disk_completed_);
  EXPECT_TRUE(ranker.Rank().empty());

  ExpectErrors(/* fresh_model_created = */ true,
               /* using_fake_predictor = */ true);
}

TEST_F(RecurrenceRankerTest, SaveToDisk) {
  auto ranker = MakeSimpleRanker();

  // Sanity checks
  ASSERT_TRUE(ranker->load_from_disk_completed_);
  EXPECT_TRUE(ranker->Rank().empty());

  // Check the ranker file should have been created on initialization.
  EXPECT_TRUE(base::PathExists(ranker_filepath_));

  // Make the ranker do a save.
  ranker->Record("A");
  ranker->Record("B");
  ranker->Record("B");
  ranker->Record("C");
  ranker->SaveToDisk();
  Wait();

  // Check the ranker file is created.
  EXPECT_TRUE(base::PathExists(ranker_filepath_));

  // Parse the content of the file.
  std::string str_written;
  EXPECT_TRUE(base::ReadFileToString(ranker_filepath_, &str_written));
  RecurrenceRankerProto proto_written;
  EXPECT_TRUE(proto_written.ParseFromString(str_written));

  // Expect the content to be proto_.
  EXPECT_TRUE(EquivToProtoLite(proto_written, MakeTestingProto()));

  ExpectErrors(/* fresh_model_created = */ true,
               /* using_fake_predictor = */ true,
               /* has_saved = */ true);
}

TEST_F(RecurrenceRankerTest, SavedRankerRejectedIfConfigMismatched) {
  auto ranker = MakeSimpleRanker();

  // Make the first ranker do a save.
  ranker->Record("A");
  ranker->SaveToDisk();
  Wait();

  // Construct a second ranker with a slightly different config.
  RecurrenceRankerConfigProto other_config;
  PartiallyPopulateConfig(&other_config);
  other_config.mutable_predictor()->mutable_fake_predictor();
  other_config.set_min_seconds_between_saves(1234);

  RecurrenceRanker other_ranker("MyModel", ranker_filepath_, other_config,
                                false);
  Wait();

  // Expect that the second ranker doesn't return any rankings, because it
  // rejected the saved proto as being made by a different config.
  EXPECT_TRUE(other_ranker.load_from_disk_completed_);
  EXPECT_TRUE(other_ranker.Rank().empty());
  // For comparison:
  EXPECT_THAT(ranker->Rank(), UnorderedElementsAre(Pair("A", FloatEq(1.0f))));
  // Should also log an error to UMA.
  histogram_tester_.ExpectBucketCount(
      "RecurrenceRanker.InitializationStatus.MyModel",
      InitializationStatus::kHashMismatch, 1);
}

TEST_F(RecurrenceRankerTest, Cleanup) {
  auto ranker = MakeSimpleRanker();

  // Targets to forget.
  ranker->Record("A");
  ranker->Record("B");
  ranker->Record("C");

  // Valid proportion is 1 so cleanup shouldn't be called. Rank is called once
  // on the ranker to possibly trigger the cleanup, and a second time on the
  // predictor to observe the effects without ever triggering a cleanup.
  ranker->Rank();
  EXPECT_THAT(ranker->predictor_->Rank(0u),
              UnorderedElementsAre(Pair(0u, _), Pair(1u, _), Pair(2u, _)));

  // Record enough times that A should be removed from the ranker's store.
  for (int i = 0; i < 100; ++i) {
    ranker->Record("B");
    ranker->Record("C");
  }

  // Valid proportion is 2/3 so cleanup still shouldn't be called.
  ranker->Rank();
  EXPECT_THAT(ranker->predictor_->Rank(0u),
              UnorderedElementsAre(Pair(0u, _), Pair(1u, _), Pair(2u, _)));

  // Record enough times that B and C be removed from the ranker's store
  for (int i = 0; i < 100; ++i)
    ranker->Record("D");

  // Valid proportion is 1/4 so cleanup should be called. Examining the internal
  // state, the predictor should only contain the ID for D.
  ranker->Rank();
  EXPECT_THAT(ranker->predictor_->Rank(0u), UnorderedElementsAre(Pair(3u, _)));
}

TEST_F(RecurrenceRankerTest, EphemeralUsersUseDefaultPredictor) {
  RecurrenceRanker ephemeral_ranker("MyModel", ranker_filepath_,
                                    MakeSimpleConfig(), true);
  Wait();
  EXPECT_THAT(ephemeral_ranker.GetPredictorNameForTesting(),
              StrEq(DefaultPredictor::kPredictorName));
  histogram_tester_.ExpectBucketCount(
      "RecurrenceRanker.InitializationStatus.MyModel",
      InitializationStatus::kEphemeralUser, 1);
}

TEST_F(RecurrenceRankerTest, IntegrationWithDefaultPredictor) {
  RecurrenceRankerConfigProto config;
  PartiallyPopulateConfig(&config);
  config.mutable_predictor()->mutable_default_predictor();

  RecurrenceRanker ranker("MyModel", ranker_filepath_, config, false);
  Wait();

  ranker.Record("A");
  ranker.Record("A");
  ranker.Record("B");
  ranker.Record("C");

  EXPECT_THAT(ranker.Rank(), UnorderedElementsAre(Pair("A", FloatEq(0.2304f)),
                                                  Pair("B", FloatEq(0.16f)),
                                                  Pair("C", FloatEq(0.2f))));
  ExpectErrors(/* fresh_model_created = */ true,
               /* using_fake_predictor = */ false);
}

TEST_F(RecurrenceRankerTest, IntegrationWithZeroStateFrecencyPredictor) {
  RecurrenceRankerConfigProto config;
  PartiallyPopulateConfig(&config);
  auto* predictor = config.mutable_predictor()->mutable_frecency_predictor();
  predictor->set_decay_coeff(0.5f);

  RecurrenceRanker ranker("MyModel", ranker_filepath_, config, false);
  Wait();

  ranker.Record("A");
  ranker.Record("A");
  ranker.Record("D");
  ranker.Record("C");
  ranker.Record("E");
  ranker.RenameTarget("D", "B");
  ranker.RemoveTarget("E");
  ranker.RenameTarget("E", "A");

  // E with score 0.5 not yet removed from model.
  const float total = 0.09375f + 0.125f + 0.25f + 0.5f;
  EXPECT_THAT(ranker.Rank(),
              UnorderedElementsAre(Pair("A", FloatEq(0.09375f / total)),
                                   Pair("B", FloatEq(0.125f / total)),
                                   Pair("C", FloatEq(0.25f / total))));
  ExpectErrors(/* fresh_model_created = */ true,
               /* using_fake_predictor = */ false);
}

}  // namespace app_list
