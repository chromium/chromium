// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/ukm_data_manager_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/database/mock_ukm_database.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/model_manager_impl.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"
#include "components/segmentation_platform/internal/segmentation_platform_service_test_base.h"
#include "components/segmentation_platform/internal/signals/ukm_observer.h"
#include "components/segmentation_platform/public/features.h"
#include "components/segmentation_platform/public/local_state_helper.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace segmentation_platform {

namespace {

using testing::_;
using ukm::builders::PageLoad;
using ukm::builders::PaintPreviewCapture;

constexpr ukm::SourceId kSourceId = 10;
constexpr ukm::SourceId kSourceId2 = 20;

ukm::mojom::UkmEntryPtr GetSamplePageLoadEntry(
    ukm::SourceId source_id = kSourceId) {
  ukm::mojom::UkmEntryPtr entry = ukm::mojom::UkmEntry::New();
  entry->source_id = source_id;
  entry->event_hash = PageLoad::kEntryNameHash;
  entry->metrics[PageLoad::kCpuTimeNameHash] = 10;
  entry->metrics[PageLoad::kIsNewBookmarkNameHash] = 20;
  entry->metrics[PageLoad::kIsNTPCustomLinkNameHash] = 30;
  return entry;
}

ukm::mojom::UkmEntryPtr GetSamplePaintPreviewEntry(
    ukm::SourceId source_id = kSourceId) {
  ukm::mojom::UkmEntryPtr entry = ukm::mojom::UkmEntry::New();
  entry->source_id = source_id;
  entry->event_hash = PaintPreviewCapture::kEntryNameHash;
  entry->metrics[PaintPreviewCapture::kBlinkCaptureTimeNameHash] = 5;
  entry->metrics[PaintPreviewCapture::kCompressedOnDiskSizeNameHash] = 15;
  return entry;
}

proto::SegmentationModelMetadata PageLoadModelMetadata() {
  proto::SegmentationModelMetadata metadata;
  metadata.set_time_unit(proto::TimeUnit::DAY);
  metadata.set_bucket_duration(42u);
  auto* feature = metadata.add_input_features();
  auto* sql_feature = feature->mutable_sql_feature();
  sql_feature->set_sql("SELECT COUNT(*) from metrics;");

  auto* ukm_event = sql_feature->mutable_signal_filter()->add_ukm_events();
  ukm_event->set_event_hash(PageLoad::kEntryNameHash);
  ukm_event->add_metric_hash_filter(PageLoad::kCpuTimeNameHash);
  ukm_event->add_metric_hash_filter(PageLoad::kIsNewBookmarkNameHash);
  return metadata;
}

proto::SegmentationModelMetadata PaintPreviewModelMetadata() {
  proto::SegmentationModelMetadata metadata;
  metadata.set_time_unit(proto::TimeUnit::DAY);
  metadata.set_bucket_duration(42u);

  auto* feature = metadata.add_input_features();
  auto* sql_feature = feature->mutable_sql_feature();
  sql_feature->set_sql("SELECT COUNT(*) from metrics;");
  auto* ukm_event2 = sql_feature->mutable_signal_filter()->add_ukm_events();
  ukm_event2->set_event_hash(PaintPreviewCapture::kEntryNameHash);
  ukm_event2->add_metric_hash_filter(
      PaintPreviewCapture::kBlinkCaptureTimeNameHash);
  return metadata;
}

}  // namespace

class TestServicesForPlatform : public SegmentationPlatformServiceTestBase {
 public:
  explicit TestServicesForPlatform(UkmDataManagerImpl* ukm_data_manager) {
    EXPECT_TRUE(profile_dir.CreateUniqueTempDir());
    history_service = history::CreateHistoryService(profile_dir.GetPath(),
                                                    /*create_db=*/true);

    InitPlatform(ukm_data_manager, history_service.get());

    segment_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    signal_db_->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
    segment_storage_config_db_->InitStatusCallback(
        leveldb_proto::Enums::InitStatus::kOK);
    signal_db_->LoadCallback(true);
    segment_storage_config_db_->LoadCallback(true);

    // If initialization is succeeded, model execution scheduler should start
    // querying segment db.
    segment_db_->LoadCallback(true);
  }

  ~TestServicesForPlatform() override {
    DestroyPlatform();
    history_service.reset();
  }

  void AddModel(const proto::SegmentationModelMetadata& metadata) {
    auto& callback = model_provider_data_.model_providers_callbacks
                         [SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE];
    callback.Run(SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE, metadata,
                 0);
    segment_db_->UpdateCallback(true);
    base::RunLoop().RunUntilIdle();
  }

  SegmentationPlatformServiceImpl& platform() {
    return *segmentation_platform_service_impl_;
  }

  void SaveSegmentResult(SegmentId segment_id,
                         std::optional<proto::PredictionResult> result) {
    const std::string key = base::NumberToString(static_cast<int>(segment_id));
    auto& segment_info = segment_db_entries_[key];
    // Assume that test already created the segment info, this method only
    // writes result.
    ASSERT_EQ(segment_info.segment_id(), segment_id);
    if (result) {
      *segment_info.mutable_prediction_result() = std::move(*result);
    } else {
      segment_info.clear_prediction_result();
    }
  }

  bool HasSegmentResult(SegmentId segment_id) {
    const std::string key = base::NumberToString(static_cast<int>(segment_id));
    const auto it = segment_db_entries_.find(key);
    if (it == segment_db_entries_.end())
      return false;
    return it->second.has_prediction_result();
  }

  base::ScopedTempDir profile_dir;
  std::unique_ptr<history::HistoryService> history_service;
};

class UkmDataManagerImplTest : public testing::Test {
 public:
  UkmDataManagerImplTest() = default;
  ~UkmDataManagerImplTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        features::kSegmentationPlatformSignalDbCache);
    SegmentationPlatformService::RegisterLocalStatePrefs(prefs_.registry());
    LocalStateHelper::GetInstance().Initialize(&prefs_);
    ukm_recorder_ = std::make_unique<ukm::TestUkmRecorder>();
    auto ukm_db = std::make_unique<MockUkmDatabase>();
    ukm_database_ = ukm_db.get();
    ukm_observer_ = std::make_unique<UkmObserver>(ukm_recorder_.get());
    data_manager_ = std::make_unique<UkmDataManagerImpl>();
    data_manager_->InitializeForTesting(std::move(ukm_db), ukm_observer_.get());
  }

  void TearDown() override {
    ukm_database_ = nullptr;
    data_manager_.reset();
    ukm_observer_.reset();
    ukm_recorder_.reset();
  }

  void RecordUkmAndWaitForDatabase(ukm::mojom::UkmEntryPtr entry) {}

  TestServicesForPlatform& CreatePlatform() {
    platform_services_.push_back(
        std::make_unique<TestServicesForPlatform>(data_manager_.get()));
    return *platform_services_.back();
  }

  void RemovePlatform(const TestServicesForPlatform* platform) {
    auto it = platform_services_.begin();
    while (it != platform_services_.end()) {
      if (it->get() == platform) {
        platform_services_.erase(it);
        return;
      }
      it++;
    }
  }

 protected:
  // Use system time to avoid history service expiration tasks to go into an
  // infinite loop.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::SYSTEM_TIME};

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<UkmObserver> ukm_observer_;
  std::unique_ptr<ukm::TestUkmRecorder> ukm_recorder_;
  raw_ptr<MockUkmDatabase> ukm_database_;
  std::unique_ptr<UkmDataManagerImpl> data_manager_;
  std::vector<std::unique_ptr<TestServicesForPlatform>> platform_services_;
  std::vector<ukm::mojom::UkmEntryPtr> db_entries_;
  TestingPrefServiceSimple prefs_;
};

MATCHER_P(HasEventHash, event_hash, "") {
  return arg->event_hash == event_hash;
}

TEST_F(UkmDataManagerImplTest, HistoryNotification) {
  const GURL kUrl1 = GURL("https://www.url1.com/");
  const SegmentId kSegmentId =
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE;

  TestServicesForPlatform& platform1 = CreatePlatform();
  platform1.AddModel(PageLoadModelMetadata());
  proto::PredictionResult prediction_result;
  prediction_result.add_result(10);
  prediction_result.set_timestamp_us(1000);
  platform1.SaveSegmentResult(kSegmentId, prediction_result);
  EXPECT_TRUE(platform1.HasSegmentResult(kSegmentId));

  // Add a page to history and check that the notification is sent to
  // UkmDatabase. All notifications should be sent.
  base::RunLoop wait_for_add1;
  EXPECT_CALL(*ukm_database_, OnUrlValidated(kUrl1, kTestProfileId))
      .WillOnce([&wait_for_add1]() { wait_for_add1.QuitClosure().Run(); });
  platform1.history_service->AddPage(kUrl1, base::Time::Now(),
                                     history::VisitSource::SOURCE_BROWSED);
  wait_for_add1.Run();

  platform1.history_service->DeleteURLs({kUrl1});

  // Check that RemoveUrls() notification is sent to UkmDatabase.
  base::RunLoop wait_for_remove1;
  EXPECT_CALL(*ukm_database_, RemoveUrls(std::vector({kUrl1}), false))
      .WillOnce(
          [&wait_for_remove1]() { wait_for_remove1.QuitClosure().Run(); });
  wait_for_remove1.Run();

  // Run segment info callbacks that were posted to remove results.
  platform1.segment_db().UpdateCallback(true);

  // History based segment results should be removed.
  EXPECT_FALSE(platform1.HasSegmentResult(
      SegmentId::OPTIMIZATION_TARGET_SEGMENTATION_SHARE));

  RemovePlatform(&platform1);
}

TEST_F(UkmDataManagerImplTest, UkmSourceObservation) {
  const GURL kUrl1 = GURL("https://www.url1.com/");

  // Create a platform that observes PageLoad events.
  TestServicesForPlatform& platform1 = CreatePlatform();
  platform1.AddModel(PageLoadModelMetadata());

  // Source updates are notified to the database.
  base::RunLoop wait_for_source;
  EXPECT_CALL(*ukm_database_,
              UpdateUrlForUkmSource(kSourceId, kUrl1, /*is_validated=*/false,
                                    /*profile_id*/ ""))
      .WillOnce([&wait_for_source](ukm::SourceId source_id, const GURL& url,
                                   bool is_validated,
                                   const std::string& profile_id) {
        wait_for_source.QuitClosure().Run();
      });
  ukm_recorder_->UpdateSourceURL(kSourceId, kUrl1);
  wait_for_source.Run();

  RemovePlatform(&platform1);
}

TEST_F(UkmDataManagerImplTest, UkmEntryObservation) {
  const GURL kUrl1 = GURL("https://www.url1.com/");

  // UKM added before creating platform do not get recorded.
  ukm_recorder_->AddEntry(GetSamplePageLoadEntry());
  ukm_recorder_->AddEntry(GetSamplePaintPreviewEntry());

  // Create a platform that observes PageLoad events.
  TestServicesForPlatform& platform1 = CreatePlatform();
  platform1.AddModel(PageLoadModelMetadata());

  // Not added since UkmDataManager is not notified for UKM observation.
  ukm_recorder_->AddEntry(GetSamplePageLoadEntry());

  // Not added since it is not PageLoad event.
  ukm_recorder_->AddEntry(GetSamplePaintPreviewEntry());

  // PageLoad event gets recorded in UkmDatabase.
  base::RunLoop wait_for_record;
  EXPECT_CALL(*ukm_database_,
              StoreUkmEntry(HasEventHash(PageLoad::kEntryNameHash)))
      .WillOnce([&wait_for_record](ukm::mojom::UkmEntryPtr entry) {
        wait_for_record.QuitClosure().Run();
      });
  ukm_recorder_->AddEntry(GetSamplePageLoadEntry());
  wait_for_record.Run();

  RemovePlatform(&platform1);
}

TEST_F(UkmDataManagerImplTest, UkmServiceCreatedBeforePlatform) {
  const GURL kUrl1 = GURL("https://www.url1.com/");

  TestServicesForPlatform& platform1 = CreatePlatform();
  platform1.AddModel(PageLoadModelMetadata());

  // Entry should be recorded, This step does not wait for the database record
  // here since it is waits for the next observation below.
  EXPECT_CALL(*ukm_database_,
              StoreUkmEntry(HasEventHash(PageLoad::kEntryNameHash)));
  ukm_recorder_->AddEntry(GetSamplePageLoadEntry());

  // Source updates should be notified.
  base::RunLoop wait_for_source;
  EXPECT_CALL(*ukm_database_,
              UpdateUrlForUkmSource(kSourceId, kUrl1, /*is_validated=*/false,
                                    /*profile_id*/ ""))
      .WillOnce([&wait_for_source](ukm::SourceId source_id, const GURL& url,
                                   bool is_validated,
                                   const std::string& profile_id) {
        wait_for_source.QuitClosure().Run();
      });
  ukm_recorder_->UpdateSourceURL(kSourceId, kUrl1);
  wait_for_source.Run();

  RemovePlatform(&platform1);
}

TEST_F(UkmDataManagerImplTest, UrlValidationWithHistory) {
  const GURL kUrl1 = GURL("https://www.url1.com/");

  TestServicesForPlatform& platform1 = CreatePlatform();
  platform1.AddModel(PageLoadModelMetadata());

  // History page is added before source update.
  base::RunLoop wait_for_add1;
  EXPECT_CALL(*ukm_database_, OnUrlValidated(kUrl1, kTestProfileId))
      .WillOnce([&wait_for_add1]() { wait_for_add1.QuitClosure().Run(); });
  platform1.history_service->AddPage(kUrl1, base::Time::Now(),
                                     history::VisitSource::SOURCE_BROWSED);
  wait_for_add1.Run();

  // Source update should have a validated URL.
  base::RunLoop wait_for_source;
  EXPECT_CALL(*ukm_database_,
              UpdateUrlForUkmSource(kSourceId, kUrl1, /*is_validated=*/true,
                                    kTestProfileId))
      .WillOnce([&wait_for_source](ukm::SourceId source_id, const GURL& url,
                                   bool is_validated,
                                   const std::string& profile_id) {
        wait_for_source.QuitClosure().Run();
      });
  ukm_recorder_->UpdateSourceURL(kSourceId, kUrl1);
  wait_for_source.Run();

  RemovePlatform(&platform1);
}

TEST_F(UkmDataManagerImplTest, MultiplePlatforms) {
  const GURL kUrl1 = GURL("https://www.url1.com/");
  const GURL kUrl2 = GURL("https://www.url2.com/");

  // Create 2 platforms, and 1 of them observing UKM events.
  TestServicesForPlatform& platform1 = CreatePlatform();
  TestServicesForPlatform& platform3 = CreatePlatform();
  platform1.AddModel(PageLoadModelMetadata());

  // Only page load should be added to database.
  EXPECT_CALL(*ukm_database_,
              StoreUkmEntry(HasEventHash(PageLoad::kEntryNameHash)));
  ukm_recorder_->AddEntry(GetSamplePageLoadEntry());
  ukm_recorder_->AddEntry(GetSamplePaintPreviewEntry());

  // Create another platform observing paint preview.
  TestServicesForPlatform& platform2 = CreatePlatform();
  platform2.AddModel(PaintPreviewModelMetadata());

  // Both should be added to database.
  EXPECT_CALL(*ukm_database_,
              StoreUkmEntry(HasEventHash(PageLoad::kEntryNameHash)));
  EXPECT_CALL(*ukm_database_,
              StoreUkmEntry(HasEventHash(PaintPreviewCapture::kEntryNameHash)));
  ukm_recorder_->AddEntry(GetSamplePageLoadEntry());
  ukm_recorder_->AddEntry(GetSamplePaintPreviewEntry());

  // Sources should still be updated.
  base::RunLoop wait_for_source;
  EXPECT_CALL(*ukm_database_,
              UpdateUrlForUkmSource(kSourceId, kUrl1, /*is_validated=*/false,
                                    /*profile_id*/ ""))
      .WillOnce([&wait_for_source](ukm::SourceId source_id, const GURL& url,
                                   bool is_validated,
                                   const std::string& profile_id) {
        wait_for_source.QuitClosure().Run();
      });
  ukm_recorder_->UpdateSourceURL(kSourceId, kUrl1);
  wait_for_source.Run();

  // Removing platform1 does not stop observing metrics.
  RemovePlatform(&platform1);
  EXPECT_CALL(*ukm_database_,
              StoreUkmEntry(HasEventHash(PageLoad::kEntryNameHash)));
  EXPECT_CALL(*ukm_database_,
              StoreUkmEntry(HasEventHash(PaintPreviewCapture::kEntryNameHash)));
  ukm_recorder_->AddEntry(GetSamplePageLoadEntry());
  ukm_recorder_->AddEntry(GetSamplePaintPreviewEntry());

  // Update history service on one of the platforms, and the database should get
  // a validated URL.
  base::RunLoop wait_for_add1;
  EXPECT_CALL(*ukm_database_, OnUrlValidated(kUrl2, kTestProfileId))
      .WillOnce([&wait_for_add1]() { wait_for_add1.QuitClosure().Run(); });
  platform2.history_service->AddPage(kUrl2, base::Time::Now(),
                                     history::VisitSource::SOURCE_BROWSED);
  wait_for_add1.Run();

  base::RunLoop wait_for_source2;
  EXPECT_CALL(*ukm_database_,
              UpdateUrlForUkmSource(kSourceId2, kUrl2, /*is_validated=*/true,
                                    kTestProfileId))
      .WillOnce([&wait_for_source2](ukm::SourceId source_id, const GURL& url,
                                    bool is_validated,
                                    const std::string& profile_id) {
        wait_for_source2.QuitClosure().Run();
      });
  ukm_recorder_->UpdateSourceURL(kSourceId2, kUrl2);
  wait_for_source2.Run();

  RemovePlatform(&platform2);
  RemovePlatform(&platform3);
}

}  // namespace segmentation_platform
