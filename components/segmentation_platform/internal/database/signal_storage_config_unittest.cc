// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/signal_storage_config.h"

#include "base/memory/raw_ptr.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "components/segmentation_platform/internal/proto/signal_storage_config.pb.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using InitStatus = leveldb_proto::Enums::InitStatus;

namespace segmentation_platform {

namespace {
const char kDatabaseKey[] = "config";

}  // namespace

class SignalStorageConfigTest : public testing::Test {
 public:
  SignalStorageConfigTest() = default;
  ~SignalStorageConfigTest() override = default;

 protected:
  void SetUpDB() {
    DCHECK(!db_);
    DCHECK(!signal_storage_config_);

    auto db = std::make_unique<
        leveldb_proto::test::FakeDB<proto::SignalStorageConfigs>>(&db_entries_);
    db_ = db.get();
    signal_storage_config_ =
        std::make_unique<SignalStorageConfig>(std::move(db), &test_clock_);
    test_clock_.SetNow(base::Time::Now());
  }

  void TearDown() override {
    db_entries_.clear();
    db_ = nullptr;
    signal_storage_config_.reset();
  }

  base::test::TaskEnvironment task_environment_;
  base::SimpleTestClock test_clock_;
  std::map<std::string, proto::SignalStorageConfigs> db_entries_;
  raw_ptr<leveldb_proto::test::FakeDB<proto::SignalStorageConfigs>> db_{
      nullptr};
  std::unique_ptr<SignalStorageConfig> signal_storage_config_;
};

TEST_F(SignalStorageConfigTest,
       CheckMeetsSignalCollectionRequirementWithMultipleModels) {
  // TODO(haileywang): Separate UMA and UKM features tests. Make smaller helper
  // functions that can be used by other tests to share repeated code. Start
  // with empty DB.
  SetUpDB();
  base::MockCallback<SignalStorageConfig::SuccessCallback> init_callback;
  EXPECT_CALL(init_callback, Run(true)).Times(1);
  signal_storage_config_->InitAndLoad(init_callback.Get());
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  db_->LoadCallback(true);
  EXPECT_EQ(0u, db_entries_.size());

  // Create a model metadata.
  proto::SegmentationModelMetadata metadata;
  metadata.set_time_unit(proto::TimeUnit::DAY);
  metadata.set_signal_storage_length(2);
  metadata.set_min_signal_collection_length(2);

  // Create a second model metadata with longer requirement.
  proto::SegmentationModelMetadata metadata2;
  metadata2.set_time_unit(proto::TimeUnit::DAY);
  metadata2.set_signal_storage_length(6);
  metadata2.set_min_signal_collection_length(4);

  // Add a user action feature to the both models.
  proto::InputFeature* input_feature = metadata.add_input_features();
  proto::UMAFeature* feature = input_feature->mutable_uma_feature();
  uint64_t name_hash = base::HashMetricName("some user action");
  feature->set_type(proto::SignalType::USER_ACTION);
  feature->set_name_hash(name_hash);
  feature->set_bucket_count(1);
  feature->set_tensor_length(1);
  feature->set_aggregation(proto::Aggregation::COUNT);
  proto::InputFeature* input_feature2 = metadata2.add_input_features();
  proto::UMAFeature* feature2 = input_feature2->mutable_uma_feature();
  feature2->set_type(proto::SignalType::USER_ACTION);
  feature2->set_name_hash(name_hash);
  feature2->set_bucket_count(1);
  feature2->set_tensor_length(1);
  feature2->set_aggregation(proto::Aggregation::COUNT);

  // Add a sql feature to both models.
  proto::InputFeature* input_feature3 = metadata.add_input_features();
  proto::SqlFeature* feature3 = input_feature3->mutable_sql_feature();
  proto::SignalFilterConfig::UkmEvent* ukm_event =
      feature3->mutable_signal_filter()->add_ukm_events();
  uint64_t event_hash = base::HashMetricName("some event");
  uint64_t metric_hash = base::HashMetricName("some metric");
  feature3->set_sql("sql");
  ukm_event->set_event_hash(event_hash);
  ukm_event->add_metric_hash_filter(metric_hash);

  proto::InputFeature* input_feature4 = metadata2.add_input_features();
  proto::SqlFeature* feature4 = input_feature4->mutable_sql_feature();
  proto::SignalFilterConfig::UkmEvent* ukm_event2 =
      feature4->mutable_signal_filter()->add_ukm_events();
  feature4->set_sql("sql");
  ukm_event2->set_event_hash(event_hash);
  ukm_event2->add_metric_hash_filter(metric_hash);

  // The DB should be empty before the model is added.
  EXPECT_EQ(0u, db_entries_.size());

  // Add the model.
  EXPECT_FALSE(
      signal_storage_config_->MeetsSignalCollectionRequirement(metadata));
  signal_storage_config_->OnSignalCollectionStarted(metadata);
  db_->UpdateCallback(true);

  // Verify that the DB has now a top level entry.
  EXPECT_EQ(1u, db_entries_.size());
  const auto& config = db_entries_[kDatabaseKey];
  EXPECT_EQ(2, config.signals_size());

  // Verify that DB has a signal entry with correct storage and collection start
  // time.
  proto::SignalStorageConfig signal_config = config.signals(0);
  EXPECT_EQ(name_hash, signal_config.name_hash());
  EXPECT_EQ(proto::SignalType::USER_ACTION, signal_config.signal_type());
  EXPECT_EQ(base::Days(2).InSeconds(), signal_config.storage_length_s());
  EXPECT_NE(0, signal_config.collection_start_time_s());

  proto::SignalStorageConfig signal_config2 = config.signals(1);
  EXPECT_EQ(metric_hash, signal_config2.name_hash());
  EXPECT_EQ(event_hash, signal_config2.event_hash());
  EXPECT_EQ(proto::SignalType::UKM_EVENT, signal_config2.signal_type());
  EXPECT_EQ(base::Days(2).InSeconds(), signal_config2.storage_length_s());
  EXPECT_NE(0, signal_config2.collection_start_time_s());

  // Add the second model. It should do a overwrite of previous value.
  signal_storage_config_->OnSignalCollectionStarted(metadata2);
  db_->UpdateCallback(true);

  // Verify DB size.
  EXPECT_EQ(1u, db_entries_.size());
  EXPECT_EQ(2, config.signals_size());

  // Verify that DB has a signal entry with correct storage and collection start
  // time.
  signal_config = config.signals(0);
  EXPECT_EQ(name_hash, signal_config.name_hash());
  EXPECT_EQ(proto::SignalType::USER_ACTION, signal_config.signal_type());
  EXPECT_EQ(base::Days(6).InSeconds(), signal_config.storage_length_s());
  EXPECT_NE(0, signal_config.collection_start_time_s());

  signal_config2 = config.signals(1);
  EXPECT_EQ(metric_hash, signal_config2.name_hash());
  EXPECT_EQ(event_hash, signal_config2.event_hash());
  EXPECT_EQ(proto::SignalType::UKM_EVENT, signal_config2.signal_type());
  EXPECT_EQ(base::Days(6).InSeconds(), signal_config2.storage_length_s());
  EXPECT_NE(0, signal_config2.collection_start_time_s());

  // Signal collection shouldn't satisfy.
  EXPECT_FALSE(
      signal_storage_config_->MeetsSignalCollectionRequirement(metadata));

  // Advance clock by 1 day. Start collection. Signal collection still won't
  // satisfy.
  test_clock_.Advance(base::Days(1));
  EXPECT_FALSE(
      signal_storage_config_->MeetsSignalCollectionRequirement(metadata));
  EXPECT_FALSE(
      signal_storage_config_->MeetsSignalCollectionRequirement(metadata2));
  EXPECT_NE(0, signal_config.collection_start_time_s());

  // Advance clock by 2 days. Signal collection should be sufficient for the
  // first model.
  test_clock_.Advance(base::Days(2));
  EXPECT_TRUE(
      signal_storage_config_->MeetsSignalCollectionRequirement(metadata));
  EXPECT_NE(0, signal_config.collection_start_time_s());

  // The second model shouldn't satisfy yet.
  EXPECT_FALSE(
      signal_storage_config_->MeetsSignalCollectionRequirement(metadata2));

  // Advance clock by 3 days. Signal collection should be sufficient for second
  // model as well.
  test_clock_.Advance(base::Days(3));
  EXPECT_TRUE(
      signal_storage_config_->MeetsSignalCollectionRequirement(metadata));
  EXPECT_TRUE(
      signal_storage_config_->MeetsSignalCollectionRequirement(metadata2));

  // Add the model several times. DB shouldn't change.
  signal_storage_config_->OnSignalCollectionStarted(metadata);
  signal_config = config.signals(0);
  EXPECT_EQ(name_hash, signal_config.name_hash());
  EXPECT_EQ(proto::SignalType::USER_ACTION, signal_config.signal_type());
  EXPECT_EQ(base::Days(6).InSeconds(), signal_config.storage_length_s());
  EXPECT_NE(0, signal_config.collection_start_time_s());

  signal_config2 = config.signals(1);
  EXPECT_EQ(metric_hash, signal_config2.name_hash());
  EXPECT_EQ(event_hash, signal_config2.event_hash());
  EXPECT_EQ(proto::SignalType::UKM_EVENT, signal_config2.signal_type());
  EXPECT_EQ(base::Days(6).InSeconds(), signal_config2.storage_length_s());
  EXPECT_NE(0, signal_config2.collection_start_time_s());
}

TEST_F(SignalStorageConfigTest, CleanupSignals) {
  SetUpDB();

  // Set up DB with three signals. One expired, one unknown, and one valid.
  proto::SignalStorageConfigs config;

  // Expired.
  proto::SignalStorageConfig* signal1 = config.add_signals();
  signal1->set_name_hash(base::HashMetricName("1"));
  signal1->set_signal_type(proto::SignalType::HISTOGRAM_VALUE);
  signal1->set_collection_start_time_s((test_clock_.Now() - base::Days(3))
                                           .ToDeltaSinceWindowsEpoch()
                                           .InSeconds());
  signal1->set_storage_length_s(base::Days(2).InSeconds());

  // Expired UKM signal.
  proto::SignalStorageConfig* ukm_signal = config.add_signals();
  ukm_signal->set_name_hash(base::HashMetricName("4"));
  ukm_signal->set_signal_type(proto::SignalType::UKM_EVENT);
  ukm_signal->set_event_hash(base::HashMetricName("4"));
  ukm_signal->set_collection_start_time_s((test_clock_.Now() - base::Days(3))
                                              .ToDeltaSinceWindowsEpoch()
                                              .InSeconds());
  ukm_signal->set_storage_length_s(base::Days(2).InSeconds());

  // Unknown.
  proto::SignalStorageConfig* signal2 = config.add_signals();
  signal2->set_name_hash(base::HashMetricName("2"));
  signal2->set_signal_type(proto::SignalType::HISTOGRAM_VALUE);
  signal2->set_collection_start_time_s((test_clock_.Now() - base::Days(3))
                                           .ToDeltaSinceWindowsEpoch()
                                           .InSeconds());
  signal2->set_storage_length_s(base::Days(5).InSeconds());

  // Known.
  proto::SignalStorageConfig* signal3 = config.add_signals();
  signal3->set_name_hash(base::HashMetricName("3"));
  signal3->set_signal_type(proto::SignalType::HISTOGRAM_VALUE);
  signal3->set_collection_start_time_s((test_clock_.Now() - base::Days(3))
                                           .ToDeltaSinceWindowsEpoch()
                                           .InSeconds());
  signal3->set_storage_length_s(base::Days(5).InSeconds());

  // Initialize non-empty DB.
  db_entries_.insert({kDatabaseKey, config});
  base::MockCallback<SignalStorageConfig::SuccessCallback> init_callback;
  EXPECT_CALL(init_callback, Run(true)).Times(1);
  signal_storage_config_->InitAndLoad(init_callback.Get());
  db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  db_->LoadCallback(true);
  EXPECT_EQ(1u, db_entries_.size());

  // Run cleanup to find expired signals
  std::set<std::pair<uint64_t, proto::SignalType>> known_signals;
  std::vector<CleanupItem> result;
  signal_storage_config_->GetSignalsForCleanup(known_signals, result);
  EXPECT_EQ(2u, result.size());
  EXPECT_EQ(base::HashMetricName("1"), result[0].name_hash);
  EXPECT_EQ(0u, result[0].event_hash);
  EXPECT_EQ(proto::SignalType::HISTOGRAM_VALUE, result[0].signal_type);

  EXPECT_EQ(base::HashMetricName("4"), result[1].name_hash);
  EXPECT_EQ(base::HashMetricName("4"), result[1].event_hash);
  EXPECT_EQ(proto::SignalType::UKM_EVENT, result[1].signal_type);

  // Run cleanup to find expired and unknown signals.
  result.clear();
  known_signals.insert(
      {base::HashMetricName("1"), proto::SignalType::HISTOGRAM_VALUE});
  known_signals.insert(
      {base::HashMetricName("3"), proto::SignalType::HISTOGRAM_VALUE});
  signal_storage_config_->GetSignalsForCleanup(known_signals, result);
  EXPECT_EQ(3u, result.size());
  EXPECT_EQ(base::HashMetricName("1"), result[0].name_hash);
  EXPECT_EQ(0u, result[0].event_hash);
  EXPECT_EQ(proto::SignalType::HISTOGRAM_VALUE, result[0].signal_type);

  EXPECT_EQ(base::HashMetricName("4"), result[1].name_hash);
  EXPECT_EQ(base::HashMetricName("4"), result[1].event_hash);
  EXPECT_EQ(proto::SignalType::UKM_EVENT, result[1].signal_type);

  EXPECT_EQ(base::HashMetricName("2"), result[2].name_hash);
  EXPECT_EQ(0u, result[2].event_hash);
  EXPECT_EQ(proto::SignalType::HISTOGRAM_VALUE, result[2].signal_type);

  // Cleanup the signals from this DB. The collection start time should be
  // updated.
  signal_storage_config_->UpdateSignalsForCleanup(result);
  db_->UpdateCallback(true);
  auto signal = db_entries_[kDatabaseKey].signals(0);
  EXPECT_EQ((test_clock_.Now() - base::Days(2))
                .ToDeltaSinceWindowsEpoch()
                .InSeconds(),
            signal.collection_start_time_s());

  auto ukm_signal1 = db_entries_[kDatabaseKey].signals(1);
  EXPECT_EQ((test_clock_.Now() - base::Days(2))
                .ToDeltaSinceWindowsEpoch()
                .InSeconds(),
            ukm_signal1.collection_start_time_s());
}

}  // namespace segmentation_platform
