// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/ukm_data_manager_impl.h"

#include "base/files/scoped_temp_dir.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/test/history_service_test_util.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/database/mock_ukm_database.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/execution/model_execution_manager_impl.h"
#include "components/segmentation_platform/internal/segmentation_platform_service_impl.h"
#include "components/segmentation_platform/internal/segmentation_platform_service_test_base.h"
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
constexpr ukm::SourceId kSourceId2 = 12;

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
    auto& callback =
        model_provider_data_.model_providers_callbacks
            [OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE];
    callback.Run(OptimizationTarget::OPTIMIZATION_TARGET_SEGMENTATION_SHARE,
                 metadata, 0);
    segment_db_->GetCallback(true);
    segment_db_->UpdateCallback(true);
    segment_db_->LoadCallback(true);
    base::RunLoop().RunUntilIdle();
  }

  SegmentationPlatformServiceImpl& platform() {
    return *segmentation_platform_service_impl_;
  }

  base::ScopedTempDir profile_dir;
  std::unique_ptr<history::HistoryService> history_service;
};

class UkmDataManagerImplTest : public testing::Test {
 public:
  UkmDataManagerImplTest() = default;
  ~UkmDataManagerImplTest() override = default;

  void SetUp() override {
    SegmentationPlatformService::RegisterLocalStatePrefs(prefs_.registry());
    data_manager_ = std::make_unique<UkmDataManagerImpl>();
    ukm_recorder_ = std::make_unique<ukm::TestUkmRecorder>();
    auto ukm_db = std::make_unique<MockUkmDatabase>();
    ukm_database_ = ukm_db.get();
    data_manager_->InitializeForTesting(std::move(ukm_db));
  }

  void TearDown() override {
    data_manager_->StopObservingUkm();
    ukm_recorder_.reset();
    ukm_database_ = nullptr;
    data_manager_.reset();
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

  TestServicesForPlatform& platform1 = CreatePlatform();

  // Add a page to history and check that the notification is sent to
  // UkmDatabase. All notifications should be sent.
  base::RunLoop wait_for_add1;
  EXPECT_CALL(*ukm_database_, OnUrlValidated(kUrl1))
      .WillOnce([&wait_for_add1]() { wait_for_add1.QuitClosure().Run(); });
  platform1.history_service->AddPage(kUrl1, base::Time::Now(),
                                     history::VisitSource::SOURCE_BROWSED);
  wait_for_add1.Run();

  platform1.history_service->DeleteURLs({kUrl1});

  // Check that RemoveUrls() notification is sent to UkmDatabase.
  base::RunLoop wait_for_remove1;
  EXPECT_CALL(*ukm_database_, RemoveUrls(std::vector({kUrl1})))
      .WillOnce(
          [&wait_for_remove1]() { wait_for_remove1.QuitClosure().Run(); });
  wait_for_remove1.Run();

  RemovePlatform(&platform1);
}

TEST_F(UkmDataManagerImplTest, UkmSourceObservation) {
  const GURL kUrl1 = GURL("https://www.url1.com/");

  data_manager_->NotifyCanObserveUkm(ukm_recorder_.get(), &prefs_);

  // Create a platform that observes PageLoad events.
  TestServicesForPlatform& platform1 = CreatePlatform();
  platform1.AddModel(PageLoadModelMetadata());

  // Source updates are notified to the database.
  base::RunLoop wait_for_source;
  EXPECT_CALL(*ukm_database_,
              UpdateUrlForUkmSource(kSourceId, kUrl1, /*is_validated=*/false))
      .WillOnce([&wait_for_source](ukm::SourceId source_id, const GURL& url,
                                   bool is_validated) {
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

  data_manager_->NotifyCanObserveUkm(ukm_recorder_.get(), &prefs_);

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

  // Observation is available before platforms are created.
  data_manager_->NotifyCanObserveUkm(ukm_recorder_.get(), &prefs_);

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
              UpdateUrlForUkmSource(kSourceId, kUrl1, /*is_validated=*/false))
      .WillOnce([&wait_for_source](ukm::SourceId source_id, const GURL& url,
                                   bool is_validated) {
        wait_for_source.QuitClosure().Run();
      });
  ukm_recorder_->UpdateSourceURL(kSourceId, kUrl1);
  wait_for_source.Run();

  RemovePlatform(&platform1);
}

TEST_F(UkmDataManagerImplTest, UrlValidationWithHistory) {
  const GURL kUrl1 = GURL("https://www.url1.com/");

  data_manager_->NotifyCanObserveUkm(ukm_recorder_.get(), &prefs_);
  TestServicesForPlatform& platform1 = CreatePlatform();
  platform1.AddModel(PageLoadModelMetadata());

  // History page is added before source update.
  base::RunLoop wait_for_add1;
  EXPECT_CALL(*ukm_database_, OnUrlValidated(kUrl1))
      .WillOnce([&wait_for_add1]() { wait_for_add1.QuitClosure().Run(); });
  platform1.history_service->AddPage(kUrl1, base::Time::Now(),
                                     history::VisitSource::SOURCE_BROWSED);
  wait_for_add1.Run();

  // Source update should have a validated URL.
  base::RunLoop wait_for_source;
  EXPECT_CALL(*ukm_database_,
              UpdateUrlForUkmSource(kSourceId, kUrl1, /*is_validated=*/true))
      .WillOnce([&wait_for_source](ukm::SourceId source_id, const GURL& url,
                                   bool is_validated) {
        wait_for_source.QuitClosure().Run();
      });
  ukm_recorder_->UpdateSourceURL(kSourceId, kUrl1);
  wait_for_source.Run();

  RemovePlatform(&platform1);
}

TEST_F(UkmDataManagerImplTest, MultiplePlatforms) {
  const GURL kUrl1 = GURL("https://www.url1.com/");
  const GURL kUrl2 = GURL("https://www.url2.com/");

  data_manager_->NotifyCanObserveUkm(ukm_recorder_.get(), &prefs_);

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
              UpdateUrlForUkmSource(kSourceId, kUrl1, /*is_validated=*/false))
      .WillOnce([&wait_for_source](ukm::SourceId source_id, const GURL& url,
                                   bool is_validated) {
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
  EXPECT_CALL(*ukm_database_, OnUrlValidated(kUrl2))
      .WillOnce([&wait_for_add1]() { wait_for_add1.QuitClosure().Run(); });
  platform2.history_service->AddPage(kUrl2, base::Time::Now(),
                                     history::VisitSource::SOURCE_BROWSED);
  wait_for_add1.Run();

  base::RunLoop wait_for_source2;
  EXPECT_CALL(*ukm_database_,
              UpdateUrlForUkmSource(kSourceId2, kUrl2, /*is_validated=*/true))
      .WillOnce([&wait_for_source2](ukm::SourceId source_id, const GURL& url,
                                    bool is_validated) {
        wait_for_source2.QuitClosure().Run();
      });
  ukm_recorder_->UpdateSourceURL(kSourceId2, kUrl2);
  wait_for_source2.Run();

  RemovePlatform(&platform2);
  RemovePlatform(&platform3);
}

// Tests that the most recent time for UKM allowed state is correctly set and
// read.
TEST_F(UkmDataManagerImplTest, GetUkmMostRecentAllowedTime) {
  // Without PrefService, base::Time::Max() will be returned.
  data_manager_->OnUkmAllowedStateChanged(true);
  EXPECT_EQ(data_manager_->GetUkmMostRecentAllowedTime(), base::Time::Max());

  data_manager_->NotifyCanObserveUkm(ukm_recorder_.get(), &prefs_);
  EXPECT_LE(data_manager_->GetUkmMostRecentAllowedTime(), base::Time::Now());

  // Change the allowed state to false, the start time should now be set to
  // Time::Max().
  ukm_recorder_->OnUkmAllowedStateChanged(false);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::Time::Max(), data_manager_->GetUkmMostRecentAllowedTime());

  // Change the allowed state to true, the new start time should be close to
  // now.
  base::Time now = base::Time::Now();
  ukm_recorder_->OnUkmAllowedStateChanged(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_LE(now, data_manager_->GetUkmMostRecentAllowedTime());
  EXPECT_LE(data_manager_->GetUkmMostRecentAllowedTime(), base::Time::Now());
}

}  // namespace segmentation_platform
