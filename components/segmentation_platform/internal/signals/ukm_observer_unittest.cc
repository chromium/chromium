// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/signals/ukm_observer.h"

#include <memory>

#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/segmentation_platform/internal/constants.h"
#include "components/segmentation_platform/internal/database/mock_ukm_database.h"
#include "components/segmentation_platform/internal/signals/ukm_config.h"
#include "components/segmentation_platform/internal/signals/url_signal_handler.h"
#include "components/segmentation_platform/internal/ukm_data_manager_impl.h"
#include "components/segmentation_platform/public/local_state_helper.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace segmentation_platform {

namespace {

using testing::_;
using testing::Invoke;
using testing::UnorderedElementsAre;
using ukm::builders::PageLoad;
using ukm::builders::PaintPreviewCapture;

constexpr ukm::SourceId kSourceId = 10;

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

UkmEventHash TestEvent(uint64_t hash) {
  return UkmEventHash::FromUnsafeValue(hash);
}

UkmMetricHash TestMetric(uint64_t hash) {
  return UkmMetricHash::FromUnsafeValue(hash);
}

}  // namespace

class UkmObserverTest : public testing::Test {
 public:
  UkmObserverTest() = default;
  ~UkmObserverTest() override = default;

  void SetUp() override {
    SegmentationPlatformService::RegisterLocalStatePrefs(prefs_.registry());
    LocalStateHelper::GetInstance().Initialize(&prefs_);
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
    InitializeUkmObserver({ukm::MSBB} /*consent_state*/);
  }

  void TearDown() override {
    ukm_data_manager_.reset();
    ukm_observer_.reset();
    ukm_recorder_.reset();
  }

  void ExpectUkmEventFromRecorder(ukm::mojom::UkmEntryPtr entry) {
    uint64_t event_hash = entry->event_hash;
    base::RunLoop wait_for_record;
    EXPECT_CALL(ukm_database(), StoreUkmEntry(_))
        .WillOnce(
            [&wait_for_record, &event_hash](ukm::mojom::UkmEntryPtr ukm_entry) {
              EXPECT_EQ(ukm_entry->event_hash, event_hash);
              wait_for_record.QuitClosure().Run();
            });
    ukm_recorder_->AddEntry(std::move(entry));
    wait_for_record.Run();
  }

  void InitializeUkmObserver(ukm::UkmConsentState consent_state) {
    ukm_data_manager_.reset();
    ukm_observer_.reset();
    ukm_observer_ = std::make_unique<UkmObserver>(ukm_recorder_.get());
    ukm_observer_->OnUkmAllowedStateChanged(consent_state);
    auto ukm_database = std::make_unique<MockUkmDatabase>();
    ukm_database_ = ukm_database.get();
    ukm_data_manager_ = std::make_unique<UkmDataManagerImpl>();
    ukm_data_manager_->InitializeForTesting(std::move(ukm_database),
                                            ukm_observer_.get());
  }

  UkmObserver& ukm_observer() { return *ukm_observer_; }
  ukm::TestUkmRecorder& ukm_recorder() { return *ukm_recorder_; }
  MockUkmDatabase& ukm_database() { return *ukm_database_; }
  UkmDataManagerImpl& ukm_data_manager() { return *ukm_data_manager_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<ukm::TestUkmRecorder> ukm_recorder_;
  std::unique_ptr<UkmObserver> ukm_observer_;
  raw_ptr<MockUkmDatabase, DanglingUntriaged> ukm_database_;
  std::unique_ptr<UkmDataManagerImpl> ukm_data_manager_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(UkmObserverTest, EmptyConfig) {
  UkmObserver& observer = ukm_observer();

  // Empty config should not add anything to database.
  observer.StartObserving(UkmConfig());
  EXPECT_CALL(ukm_database(), StoreUkmEntry(_)).Times(0);

  observer.OnEntryAdded(GetSamplePageLoadEntry());
  observer.OnEntryAdded(GetSamplePaintPreviewEntry());

  // Add page load config with no metrics.
  UkmConfig config;
  config.AddEvent(TestEvent(PageLoad::kEntryNameHash), {});
  observer.StartObserving(UkmConfig());
  observer.OnEntryAdded(GetSamplePageLoadEntry());
}

TEST_F(UkmObserverTest, FilterEventsAndMetrics) {
  UkmObserver& observer = ukm_observer();

  // Add PageLoad to the config.
  UkmConfig config1;
  config1.AddEvent(TestEvent(PageLoad::kEntryNameHash),
                   {TestMetric(PageLoad::kCpuTimeNameHash),
                    TestMetric(PageLoad::kNet_CacheBytes2NameHash),
                    TestMetric(PageLoad::kIsNewBookmarkNameHash)});
  observer.StartObserving(config1);

  EXPECT_CALL(ukm_database(), StoreUkmEntry(_))
      .WillOnce(Invoke([](ukm::mojom::UkmEntryPtr entry) {
        EXPECT_EQ(entry->event_hash, PageLoad::kEntryNameHash);
        EXPECT_THAT(entry->metrics,
                    UnorderedElementsAre(
                        std::make_pair(PageLoad::kCpuTimeNameHash, 10),
                        std::make_pair(PageLoad::kIsNewBookmarkNameHash, 20)));
      }));
  observer.OnEntryAdded(GetSamplePaintPreviewEntry());
  observer.OnEntryAdded(GetSamplePageLoadEntry());

  // Add Paint preview config to the same observer, also add metrics with wrong
  // event hashes, which should not match.
  UkmConfig config2;
  config2.AddEvent(
      TestEvent(PaintPreviewCapture::kEntryNameHash),
      {TestMetric(PaintPreviewCapture::kBlinkCaptureTimeNameHash)});
  config2.AddEvent(
      UkmEventHash(),
      {TestMetric(PaintPreviewCapture::kCompressedOnDiskSizeNameHash)});
  config2.AddEvent(
      TestEvent(PageLoad::kEntryNameHash),
      {TestMetric(PaintPreviewCapture::kCompressedOnDiskSizeNameHash)});

  // In addition to already added metrics.
  observer.StartObserving(config2);

  EXPECT_CALL(ukm_database(), StoreUkmEntry(_))
      .WillOnce(Invoke([](ukm::mojom::UkmEntryPtr entry) {
        EXPECT_EQ(entry->event_hash, PaintPreviewCapture::kEntryNameHash);
        EXPECT_THAT(entry->metrics,
                    UnorderedElementsAre(std::make_pair(
                        PaintPreviewCapture::kBlinkCaptureTimeNameHash, 5)));
      }));
  observer.OnEntryAdded(GetSamplePaintPreviewEntry());

  EXPECT_CALL(ukm_database(), StoreUkmEntry(_))
      .WillOnce(Invoke([](ukm::mojom::UkmEntryPtr entry) {
        EXPECT_EQ(entry->event_hash, PageLoad::kEntryNameHash);
        EXPECT_THAT(entry->metrics,
                    UnorderedElementsAre(
                        std::make_pair(PageLoad::kCpuTimeNameHash, 10),
                        std::make_pair(PageLoad::kIsNewBookmarkNameHash, 20)));
      }));
  observer.OnEntryAdded(GetSamplePageLoadEntry());
}

TEST_F(UkmObserverTest, PauseObservation) {
  UkmObserver& observer = ukm_observer();

  // Add PageLoad to the config.
  UkmConfig config;
  config.AddEvent(TestEvent(PageLoad::kEntryNameHash),
                  {TestMetric(PageLoad::kCpuTimeNameHash),
                   TestMetric(PageLoad::kNet_CacheBytes2NameHash),
                   TestMetric(PageLoad::kIsNewBookmarkNameHash)});
  observer.StartObserving(config);

  EXPECT_CALL(ukm_database(), StoreUkmEntry(_)).Times(0);
  EXPECT_CALL(ukm_database(), UpdateUrlForUkmSource(_, _, _, _)).Times(0);

  observer.PauseOrResumeObservation(true);

  const GURL kUrl1("https://www.url1.com");
  observer.OnEntryAdded(GetSamplePageLoadEntry());
  observer.OnUpdateSourceURL(kSourceId, {kUrl1});
}

TEST_F(UkmObserverTest, ObservationFromRecorder) {
  UkmObserver& observer = ukm_observer();
  ukm::TestUkmRecorder& recorder = ukm_recorder();

  // Add PageLoad to the config.
  UkmConfig config;
  config.AddEvent(TestEvent(PageLoad::kEntryNameHash),
                  {TestMetric(PageLoad::kCpuTimeNameHash),
                   TestMetric(PageLoad::kIsNewBookmarkNameHash)});
  ukm_data_manager().StartObservingUkm(config);

  ExpectUkmEventFromRecorder(GetSamplePageLoadEntry());

  const GURL kUrl1("https://www.url1.com");
  base::RunLoop wait_for_source;
  EXPECT_CALL(ukm_database(),
              UpdateUrlForUkmSource(kSourceId, kUrl1, /*is_validated=*/false,
                                    /*profile_id*/ ""))
      .WillOnce(
          [&wait_for_source](ukm::SourceId, const GURL&, bool, std::string) {
            wait_for_source.QuitClosure().Run();
          });
  recorder.UpdateSourceURL(kSourceId, {kUrl1});
  wait_for_source.Run();

  // Update the Config to include more events.
  UkmConfig config2;
  config2.AddEvent(
      TestEvent(PaintPreviewCapture::kEntryNameHash),
      {TestMetric(PaintPreviewCapture::kBlinkCaptureTimeNameHash)});
  observer.StartObserving(config2);

  ExpectUkmEventFromRecorder(GetSamplePageLoadEntry());
  ExpectUkmEventFromRecorder(GetSamplePaintPreviewEntry());
}

// Tests that the most recent time for UKM allowed state is correctly set and
// read.
TEST_F(UkmObserverTest, GetUkmMostRecentAllowedTime) {
  LocalStateHelper& local_state_helper = LocalStateHelper::GetInstance();
  // Without pref entry, the |is_ukm_allowed| param passed to UkmObserver ctor
  // will determine the value to be returned.
  EXPECT_LE(
      local_state_helper.GetPrefTime(kSegmentationUkmMostRecentAllowedTimeKey),
      base::Time::Now());
  InitializeUkmObserver(ukm::UkmConsentState() /*consent_state*/);
  EXPECT_EQ(base::Time::Max(), local_state_helper.GetPrefTime(
                                   kSegmentationUkmMostRecentAllowedTimeKey));

  ukm_observer().OnUkmAllowedStateChanged({ukm::MSBB});
  EXPECT_LE(
      local_state_helper.GetPrefTime(kSegmentationUkmMostRecentAllowedTimeKey),
      base::Time::Now());

  // Change the allowed state to false, the start time should now be set to
  // Time::Max().
  ukm_recorder().OnUkmAllowedStateChanged(ukm::UkmConsentState());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(base::Time::Max(), local_state_helper.GetPrefTime(
                                   kSegmentationUkmMostRecentAllowedTimeKey));

  // Change the allowed state to true, the new start time should be close to
  // now.
  base::Time now = base::Time::Now();
  ukm_recorder().OnUkmAllowedStateChanged({ukm::MSBB});
  base::RunLoop().RunUntilIdle();
  EXPECT_LE(now, local_state_helper.GetPrefTime(
                     kSegmentationUkmMostRecentAllowedTimeKey));
  EXPECT_LE(
      local_state_helper.GetPrefTime(kSegmentationUkmMostRecentAllowedTimeKey),
      base::Time::Now());
}

}  // namespace segmentation_platform
