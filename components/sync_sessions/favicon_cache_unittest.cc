// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_sessions/favicon_cache.h"

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/favicon/core/test/mock_favicon_service.h"
#include "components/sync/model/sync_change_processor_wrapper_for_test.h"
#include "components/sync/model/sync_error_factory_mock.h"
#include "components/sync/model/time.h"
#include "components/sync/protocol/favicon_image_specifics.pb.h"
#include "components/sync/protocol/favicon_tracking_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_sessions {

namespace {

using testing::_;

// Total number of favicons to use in sync test batches.
const int kFaviconBatchSize = 10;

// Maximum number of favicons to sync.
const int kMaxSyncFavicons = kFaviconBatchSize*2;

// TestChangeProcessor --------------------------------------------------------

// Dummy SyncChangeProcessor used to help review what SyncChanges are pushed
// back up to Sync.
class TestChangeProcessor : public syncer::SyncChangeProcessor {
 public:
  TestChangeProcessor();
  ~TestChangeProcessor() override;

  // Store a copy of all the changes passed in so we can examine them later.
  syncer::SyncError ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;

  syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const override {
    return syncer::SyncDataList();
  }

  bool contains_guid(const std::string& guid) const {
    return change_map_.count(guid) != 0;
  }

  syncer::SyncChange change_for_guid(const std::string& guid) const {
    DCHECK(contains_guid(guid));
    return change_map_.find(guid)->second;
  }

  // Returns the last change list received, and resets the internal list.
  syncer::SyncChangeList GetAndResetChangeList() {
    syncer::SyncChangeList list;
    list.swap(change_list_);
    return list;
  }

  void set_erroneous(bool erroneous) { erroneous_ = erroneous; }

 private:
  // Track the changes received in ProcessSyncChanges.
  std::map<std::string, syncer::SyncChange> change_map_;
  syncer::SyncChangeList change_list_;
  bool erroneous_;

  DISALLOW_COPY_AND_ASSIGN(TestChangeProcessor);
};

TestChangeProcessor::TestChangeProcessor() : erroneous_(false) {
}

TestChangeProcessor::~TestChangeProcessor() {
}

syncer::SyncError TestChangeProcessor::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  if (erroneous_) {
    return syncer::SyncError(
        FROM_HERE,
        syncer::SyncError::DATATYPE_ERROR,
        "Some error.",
        change_list[0].sync_data().GetDataType());
  }

  change_list_.insert(change_list_.end(),
                      change_list.begin(),
                      change_list.end());
  change_map_.erase(change_map_.begin(), change_map_.end());
  for (auto iter = change_list.begin(); iter != change_list.end(); ++iter) {
    change_map_[iter->sync_data().GetTitle()] = *iter;
  }
  return syncer::SyncError();
}

// TestFaviconData ------------------------------------------------------------
struct TestFaviconData {
  TestFaviconData() : is_bookmarked(false) {}
  GURL page_url;
  GURL icon_url;
  std::string image_16;
  std::string image_32;
  std::string image_64;
  base::Time last_visit_time;
  bool is_bookmarked;
};

TestFaviconData BuildFaviconData(int index) {
  TestFaviconData data;
  data.page_url = GURL(base::StringPrintf("http://bla.com/%.2i.html", index));
  data.icon_url = GURL(base::StringPrintf("http://bla.com/%.2i.ico", index));
  data.image_16 = base::StringPrintf("16 %i", index);
  // TODO(zea): enable this once the cache supports writing them.
  // data.image_32 = base::StringPrintf("32 %i", index);
  // data.image_64 = base::StringPrintf("64 %i", index);
  data.last_visit_time = syncer::ProtoTimeToTime(index);
  return data;
}

void FillImageSpecifics(
    const TestFaviconData& test_data,
    sync_pb::FaviconImageSpecifics* image_specifics) {
  image_specifics->set_favicon_url(test_data.icon_url.spec());
  if (!test_data.image_16.empty()) {
    image_specifics->mutable_favicon_web()->set_height(16);
    image_specifics->mutable_favicon_web()->set_width(16);
    image_specifics->mutable_favicon_web()->set_favicon(test_data.image_16);
  }
  if (!test_data.image_32.empty()) {
    image_specifics->mutable_favicon_web_32()->set_height(32);
    image_specifics->mutable_favicon_web_32()->set_width(32);
    image_specifics->mutable_favicon_web_32()->set_favicon(test_data.image_32);
  }
  if (!test_data.image_64.empty()) {
    image_specifics->mutable_favicon_touch_64()->set_height(64);
    image_specifics->mutable_favicon_touch_64()->set_width(64);
    image_specifics->mutable_favicon_touch_64()->
        set_favicon(test_data.image_64);
  }
}

void FillTrackingSpecifics(
    const TestFaviconData& test_data,
    sync_pb::FaviconTrackingSpecifics* tracking_specifics) {
  tracking_specifics->set_favicon_url(test_data.icon_url.spec());
  tracking_specifics->set_last_visit_time_ms(
      syncer::TimeToProtoTime(test_data.last_visit_time));
  tracking_specifics->set_is_bookmarked(test_data.is_bookmarked);
}

testing::AssertionResult CompareFaviconDataToSpecifics(
    const TestFaviconData& test_data,
    const sync_pb::EntitySpecifics& specifics) {
  if (specifics.has_favicon_image()) {
    sync_pb::FaviconImageSpecifics image_specifics = specifics.favicon_image();
    if (image_specifics.favicon_url() != test_data.icon_url.spec())
      return testing::AssertionFailure() << "Image icon url doesn't match.";
    if (!test_data.image_16.empty()) {
      if (image_specifics.favicon_web().favicon() != test_data.image_16 ||
          image_specifics.favicon_web().height() != 16 ||
          image_specifics.favicon_web().width() != 16) {
        return testing::AssertionFailure() << "16p image data doesn't match.";
      }
    } else if (image_specifics.has_favicon_web()) {
      return testing::AssertionFailure() << "Missing 16p favicon.";
    }
    if (!test_data.image_32.empty()) {
      if (image_specifics.favicon_web_32().favicon() != test_data.image_32 ||
          image_specifics.favicon_web().height() != 32 ||
          image_specifics.favicon_web().width() != 32) {
        return testing::AssertionFailure() << "32p image data doesn't match.";
      }
    } else if (image_specifics.has_favicon_web_32()) {
      return testing::AssertionFailure() << "Missing 32p favicon.";
    }
    if (!test_data.image_64.empty()) {
      if (image_specifics.favicon_touch_64().favicon() != test_data.image_64 ||
          image_specifics.favicon_web().height() != 64 ||
          image_specifics.favicon_web().width() != 64) {
        return testing::AssertionFailure() << "64p image data doesn't match.";
      }
    } else if (image_specifics.has_favicon_touch_64()) {
      return testing::AssertionFailure() << "Missing 64p favicon.";
    }
  } else {
    sync_pb::FaviconTrackingSpecifics tracking_specifics =
        specifics.favicon_tracking();
    if (tracking_specifics.favicon_url() != test_data.icon_url.spec())
      return testing::AssertionFailure() << "Tracking icon url doesn't match.";
    if (tracking_specifics.last_visit_time_ms() !=
        syncer::TimeToProtoTime(test_data.last_visit_time)) {
      return testing::AssertionFailure() << "Visit time doesn't match.";
    }
    if (tracking_specifics.is_bookmarked() != test_data.is_bookmarked)
      return testing::AssertionFailure() << "Bookmark status doesn't match.";
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult VerifyChanges(
    syncer::ModelType expected_model_type,
    const std::vector<syncer::SyncChange::SyncChangeType>&
        expected_change_types,
    const std::vector<int>& expected_icons,
    const syncer::SyncChangeList& change_list) {
  DCHECK_EQ(expected_change_types.size(), expected_icons.size());
  if (change_list.size() != expected_icons.size())
    return testing::AssertionFailure() << "Change list size doesn't match.";
  for (size_t i = 0; i < expected_icons.size(); ++i) {
    TestFaviconData data = BuildFaviconData(expected_icons[i]);
    if (change_list[i].sync_data().GetDataType() != expected_model_type)
      return testing::AssertionFailure() << "Change datatype doesn't match.";
    if (change_list[i].change_type() != expected_change_types[i])
      return testing::AssertionFailure() << "Change type doesn't match.";
    if (change_list[i].change_type() == syncer::SyncChange::ACTION_DELETE) {
      if (syncer::SyncDataLocal(change_list[i].sync_data()).GetTag() !=
          data.icon_url.spec())
        return testing::AssertionFailure() << "Deletion url does not match.";
    } else {
      testing::AssertionResult compare_result =
          CompareFaviconDataToSpecifics(
              data,
              change_list[i].sync_data().GetSpecifics());
      if (!compare_result)
        return compare_result;
    }
  }
  return testing::AssertionSuccess();
}

// Helper to extract the favicon id embedded in the tag of a sync
// change.
int GetFaviconId(const syncer::SyncChange change) {
  std::string tag = syncer::SyncDataLocal(change.sync_data()).GetTag();
  const std::string kPrefix = "http://bla.com/";
  const std::string kSuffix = ".ico";
  if (!base::StartsWith(tag, kPrefix, base::CompareCase::SENSITIVE))
    return -1;
  std::string temp = tag.substr(kPrefix.length());
  if (temp.rfind(kSuffix) <= 0)
    return -1;
  temp = temp.substr(0, temp.rfind(kSuffix));
  int result = -1;
  if (!base::StringToInt(temp, &result))
    return -1;
  return result;
}

}  // namespace

class SyncFaviconCacheTest : public testing::Test {
 public:
  SyncFaviconCacheTest();
  ~SyncFaviconCacheTest() override {}

  void SetUpInitialSync(const syncer::SyncDataList& initial_image_data,
                        const syncer::SyncDataList& initial_tracking_data);

  size_t GetFaviconCount() const;
  size_t GetTaskCount() const;

  testing::AssertionResult ExpectFaviconEquals(
        const std::string& page_url,
        const std::string& bytes) const;
  testing::AssertionResult VerifyLocalIcons(
      const std::vector<int>& expected_icons);
  testing::AssertionResult VerifyLocalCustomIcons(
      const std::vector<TestFaviconData>& expected_icons);

  std::unique_ptr<syncer::SyncChangeProcessor> CreateAndPassProcessor();
  std::unique_ptr<syncer::SyncErrorFactory> CreateAndPassSyncErrorFactory();

  FaviconCache* cache() { return &cache_; }
  TestChangeProcessor* processor() { return sync_processor_.get(); }

  // Populates the local cache of favicons in FaviconService with the data
  // described in |test_data|.
  void PopulateFaviconService(const TestFaviconData& test_data);

  // Helper method to run the message loop after invoking
  // OnReceivedSyncFavicon, which posts an internal task.
  void TriggerSyncFaviconReceived(
      const GURL& page_url,
      const GURL& icon_url,
      const std::string& icon_bytes,
      base::Time last_visit_time = base::Time::Now());

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  testing::NiceMock<favicon::MockFaviconService> mock_favicon_service_;
  FaviconCache cache_;

  // Our dummy ChangeProcessor used to inspect changes pushed to Sync.
  std::unique_ptr<TestChangeProcessor> sync_processor_;
  std::unique_ptr<syncer::SyncChangeProcessorWrapperForTest>
      sync_processor_wrapper_;
};

SyncFaviconCacheTest::SyncFaviconCacheTest()
    : task_environment_(
          base::test::SingleThreadTaskEnvironment::MainThreadType::UI),
      cache_(&mock_favicon_service_, nullptr, kMaxSyncFavicons),
      sync_processor_(new TestChangeProcessor),
      sync_processor_wrapper_(new syncer::SyncChangeProcessorWrapperForTest(
          sync_processor_.get())) {}

void SyncFaviconCacheTest::SetUpInitialSync(
    const syncer::SyncDataList& initial_image_data,
    const syncer::SyncDataList& initial_tracking_data) {
  cache()->MergeDataAndStartSyncing(syncer::FAVICON_IMAGES,
                                    initial_image_data,
                                    CreateAndPassProcessor(),
                                    CreateAndPassSyncErrorFactory());
  ASSERT_EQ(0U, processor()->GetAndResetChangeList().size());
  cache()->MergeDataAndStartSyncing(syncer::FAVICON_TRACKING,
                                    initial_tracking_data,
                                    CreateAndPassProcessor(),
                                    CreateAndPassSyncErrorFactory());
  ASSERT_EQ(0U, processor()->GetAndResetChangeList().size());
}

size_t SyncFaviconCacheTest::GetFaviconCount() const {
  return cache_.NumFaviconsForTest();
}

size_t SyncFaviconCacheTest::GetTaskCount() const {
  return cache_.NumTasksForTest();
}

testing::AssertionResult SyncFaviconCacheTest::ExpectFaviconEquals(
    const std::string& page_url,
    const std::string& bytes) const {
  GURL gurl(page_url);
  favicon_base::FaviconRawBitmapResult favicon =
      cache_.GetSyncedFaviconForPageURL(gurl);
  if (!favicon.is_valid())
    return testing::AssertionFailure() << "Favicon is missing.";
  if (favicon.bitmap_data->size() != bytes.size())
    return testing::AssertionFailure() << "Favicon sizes don't match.";
  for (size_t i = 0; i < favicon.bitmap_data->size(); ++i) {
    if (bytes[i] != *(favicon.bitmap_data->front() + i))
      return testing::AssertionFailure() << "Favicon data doesn't match.";
  }
  return testing::AssertionSuccess();
}

testing::AssertionResult SyncFaviconCacheTest::VerifyLocalIcons(
    const std::vector<int>& expected_icons) {
  std::vector<TestFaviconData> expected_custom_icons;
  for (size_t i = 0; i < expected_icons.size(); ++i) {
    expected_custom_icons.push_back(BuildFaviconData(expected_icons[i]));
  }
  return VerifyLocalCustomIcons(expected_custom_icons);
}


testing::AssertionResult SyncFaviconCacheTest::VerifyLocalCustomIcons(
    const std::vector<TestFaviconData>& expected_custom_icons) {
  syncer::SyncDataList image_data_list =
      cache()->GetAllSyncData(syncer::FAVICON_IMAGES);
  syncer::SyncDataList tracking_data_list =
      cache()->GetAllSyncData(syncer::FAVICON_TRACKING);
  if (expected_custom_icons.size() > image_data_list.size() ||
      expected_custom_icons.size() > tracking_data_list.size())
    return testing::AssertionFailure() << "Number of icons doesn't match.";
  for (size_t i = 0; i < expected_custom_icons.size(); ++i) {
    const TestFaviconData& test_data = expected_custom_icons[i];
    // Find the test data in the data lists. Assume that both lists have the
    // same ordering, which may not match the |expected_custom_icons| ordering.
    bool found_match = false;
    for (size_t j = 0; j < image_data_list.size(); ++j) {
      if (image_data_list[j].GetTitle() != test_data.icon_url.spec())
        continue;
      found_match = true;
      const sync_pb::FaviconImageSpecifics& image_specifics =
          image_data_list[j].GetSpecifics().favicon_image();
      sync_pb::FaviconImageSpecifics expected_image_specifics;
      FillImageSpecifics(test_data, &expected_image_specifics);
      if (image_specifics.SerializeAsString() !=
          expected_image_specifics.SerializeAsString()) {
        return testing::AssertionFailure() << "Image data doesn't match.";
      }
      const sync_pb::FaviconTrackingSpecifics& tracking_specifics =
          tracking_data_list[j].GetSpecifics().favicon_tracking();
      sync_pb::FaviconTrackingSpecifics expected_tracking_specifics;
      FillTrackingSpecifics(test_data, &expected_tracking_specifics);
      if (tracking_specifics.SerializeAsString() !=
          expected_tracking_specifics.SerializeAsString()) {
        return testing::AssertionFailure() << "Tracking data doesn't match.";
      }
    }
    if (!found_match)
      return testing::AssertionFailure() << "Could not find favicon.";
  }
  return testing::AssertionSuccess();
}

std::unique_ptr<syncer::SyncChangeProcessor>
SyncFaviconCacheTest::CreateAndPassProcessor() {
  return std::make_unique<syncer::SyncChangeProcessorWrapperForTest>(
      sync_processor_.get());
}

std::unique_ptr<syncer::SyncErrorFactory>
SyncFaviconCacheTest::CreateAndPassSyncErrorFactory() {
  return std::make_unique<syncer::SyncErrorFactoryMock>();
}

void SyncFaviconCacheTest::PopulateFaviconService(
    const TestFaviconData& test_data) {
  std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results;
  if (!test_data.image_16.empty()) {
    favicon_base::FaviconRawBitmapResult bitmap_result;
    bitmap_result.icon_url = test_data.icon_url;
    bitmap_result.pixel_size.set_width(16);
    bitmap_result.pixel_size.set_height(16);
    base::RefCountedString* temp_string = new base::RefCountedString();
    temp_string->data() = test_data.image_16;
    bitmap_result.bitmap_data = temp_string;
    bitmap_results.push_back(bitmap_result);
  }
  if (!test_data.image_32.empty()) {
    favicon_base::FaviconRawBitmapResult bitmap_result;
    bitmap_result.icon_url = test_data.icon_url;
    bitmap_result.pixel_size.set_width(32);
    bitmap_result.pixel_size.set_height(32);
    base::RefCountedString* temp_string = new base::RefCountedString();
    temp_string->data() = test_data.image_32;
    bitmap_result.bitmap_data = temp_string;
    bitmap_results.push_back(bitmap_result);
  }
  if (!test_data.image_64.empty()) {
    favicon_base::FaviconRawBitmapResult bitmap_result;
    bitmap_result.icon_url = test_data.icon_url;
    bitmap_result.pixel_size.set_width(64);
    bitmap_result.pixel_size.set_height(64);
    base::RefCountedString* temp_string = new base::RefCountedString();
    temp_string->data() = test_data.image_64;
    bitmap_result.bitmap_data = temp_string;
    bitmap_results.push_back(bitmap_result);
  }

  ON_CALL(mock_favicon_service_,
          GetFaviconForPageURL(test_data.page_url, _, _, _, _))
      .WillByDefault([=](auto, auto, auto,
                         favicon_base::FaviconResultsCallback callback,
                         base::CancelableTaskTracker* tracker) {
        return tracker->PostTask(
            base::ThreadTaskRunnerHandle::Get().get(), FROM_HERE,
            base::BindOnce(std::move(callback), bitmap_results));
      });
}

void SyncFaviconCacheTest::TriggerSyncFaviconReceived(
    const GURL& page_url,
    const GURL& icon_url,
    const std::string& icon_bytes,
    base::Time last_visit_time) {
  if (!icon_bytes.empty()) {
    scoped_refptr<base::RefCountedString> temp_string(
        new base::RefCountedString());
    temp_string->data() = icon_bytes;

    std::vector<favicon_base::FaviconRawBitmapResult> bitmap_results;
    bitmap_results.push_back(favicon_base::FaviconRawBitmapResult());
    bitmap_results.back().icon_url = icon_url;
    bitmap_results.back().bitmap_data = temp_string;
    bitmap_results.back().pixel_size = gfx::Size(16, 16);

    ON_CALL(mock_favicon_service_, GetFaviconForPageURL(page_url, _, _, _, _))
        .WillByDefault([=](auto, auto, auto,
                           favicon_base::FaviconResultsCallback callback,
                           base::CancelableTaskTracker* tracker) {
          return tracker->PostTask(
              base::ThreadTaskRunnerHandle::Get().get(), FROM_HERE,
              base::BindOnce(std::move(callback), bitmap_results));
        });

    // Mimic the icon itself having been cached long time ago.
    cache()->OnPageFaviconUpdated(page_url, base::Time::UnixEpoch());
    // Wait until the FaviconService replies with the result above.
    RunUntilIdle();
  }

  // Feed in the mapping.
  sync_pb::SessionTab tab;
  sync_pb::TabNavigation* navigation = tab.add_navigation();
  navigation->set_virtual_url(page_url.spec());
  navigation->set_favicon_url(icon_url.spec());
  cache()->UpdateMappingsFromForeignTab(tab, last_visit_time);
}

// A freshly constructed cache should be empty.
TEST_F(SyncFaviconCacheTest, Empty) {
  EXPECT_EQ(0U, GetFaviconCount());
}

TEST_F(SyncFaviconCacheTest, ReceiveSyncFavicon) {
  std::string page_url = "http://www.google.com";
  std::string fav_url = "http://www.google.com/favicon.ico";
  std::string bytes = "bytes";
  const base::Time visit_time = base::Time::Now();
  EXPECT_EQ(0U, GetFaviconCount());
  TriggerSyncFaviconReceived(GURL(page_url), GURL(fav_url), bytes, visit_time);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));
  EXPECT_EQ(visit_time, cache()->GetLastVisitTimeForTest(GURL(fav_url)));
}

TEST_F(SyncFaviconCacheTest, ReceiveEmptySyncFavicon) {
  std::string page_url = "http://www.google.com";
  std::string fav_url = "http://www.google.com/favicon.ico";
  std::string bytes = "bytes";
  EXPECT_EQ(0U, GetFaviconCount());
  TriggerSyncFaviconReceived(GURL(page_url), GURL(fav_url), std::string());
  EXPECT_EQ(0U, GetFaviconCount());
  EXPECT_FALSE(ExpectFaviconEquals(page_url, std::string()));

  // Then receive the actual favicon.
  TriggerSyncFaviconReceived(GURL(page_url), GURL(fav_url), bytes);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));
}

TEST_F(SyncFaviconCacheTest, ReceiveUpdatedSyncFavicon) {
  std::string page_url = "http://www.google.com";
  std::string fav_url = "http://www.google.com/favicon.ico";
  std::string bytes = "bytes";
  std::string bytes2 = "bytes2";
  EXPECT_EQ(0U, GetFaviconCount());
  TriggerSyncFaviconReceived(GURL(page_url), GURL(fav_url), bytes);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));

  // The cache should not update existing favicons from tab sync favicons
  // (which can be reassociated several times).
  TriggerSyncFaviconReceived(GURL(page_url), GURL(fav_url), bytes2);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));
  EXPECT_FALSE(ExpectFaviconEquals(page_url, bytes2));
}

TEST_F(SyncFaviconCacheTest, MultipleMappings) {
  std::string page_url = "http://www.google.com";
  std::string page2_url = "http://bla.google.com";
  std::string fav_url = "http://www.google.com/favicon.ico";
  std::string bytes = "bytes";
  EXPECT_EQ(0U, GetFaviconCount());
  TriggerSyncFaviconReceived(GURL(page_url), GURL(fav_url), bytes);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page_url, bytes));

  // Map another page to the same favicon. They should share the same data.
  TriggerSyncFaviconReceived(GURL(page2_url), GURL(fav_url), bytes);
  EXPECT_EQ(1U, GetFaviconCount());
  EXPECT_TRUE(ExpectFaviconEquals(page2_url, bytes));
}

TEST_F(SyncFaviconCacheTest, SyncEmpty) {
  syncer::SyncMergeResult merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_IMAGES,
                                        syncer::SyncDataList(),
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());

  EXPECT_EQ(0U, cache()->GetAllSyncData(syncer::FAVICON_IMAGES).size());
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(0, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(0, merge_result.num_items_before_association());
  EXPECT_EQ(0, merge_result.num_items_after_association());

  merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_TRACKING,
                                        syncer::SyncDataList(),
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());

  EXPECT_EQ(0U, cache()->GetAllSyncData(syncer::FAVICON_TRACKING).size());
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(0, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(0, merge_result.num_items_before_association());
  EXPECT_EQ(0, merge_result.num_items_after_association());
}

// Setting up sync with existing local favicons should push those favicons into
// sync.
TEST_F(SyncFaviconCacheTest, SyncExistingLocal) {
  std::vector<syncer::SyncChange::SyncChangeType> expected_change_types;
  std::vector<int> expected_icons;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    TestFaviconData favicon = BuildFaviconData(i);
    TriggerSyncFaviconReceived(favicon.page_url, favicon.icon_url,
                               favicon.image_16, syncer::ProtoTimeToTime(i));
    expected_change_types.push_back(syncer::SyncChange::ACTION_ADD);
    expected_icons.push_back(i);
  }

  syncer::SyncMergeResult merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_IMAGES,
                                        syncer::SyncDataList(),
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize),
            cache()->GetAllSyncData(syncer::FAVICON_IMAGES).size());
  syncer::SyncChangeList change_list = processor()->GetAndResetChangeList();
  EXPECT_TRUE(VerifyChanges(syncer::FAVICON_IMAGES,
                            expected_change_types,
                            expected_icons,
                            change_list));
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(0, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_before_association());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_after_association());

  merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_TRACKING,
                                        syncer::SyncDataList(),
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize),
            cache()->GetAllSyncData(syncer::FAVICON_TRACKING).size());
  change_list = processor()->GetAndResetChangeList();
  EXPECT_TRUE(VerifyChanges(syncer::FAVICON_TRACKING,
                            expected_change_types,
                            expected_icons,
                            change_list));
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(0, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_before_association());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_after_association());
}

// Setting up sync with existing sync data should load that data into the local
// cache.
TEST_F(SyncFaviconCacheTest, SyncExistingRemote) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  std::vector<int> expected_icons;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    expected_icons.push_back(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
  }

  syncer::SyncMergeResult merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_IMAGES,
                                        initial_image_data,
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize),
            cache()->GetAllSyncData(syncer::FAVICON_IMAGES).size());
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_added());
  EXPECT_EQ(0, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(0, merge_result.num_items_before_association());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_after_association());

  merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_TRACKING,
                                        initial_tracking_data,
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize),
            cache()->GetAllSyncData(syncer::FAVICON_TRACKING).size());
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_before_association());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_after_association());

  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
}

// Setting up sync with local data and sync data should merge the two image
// sets, with remote data having priority in case both exist.
TEST_F(SyncFaviconCacheTest, SyncMergesImages) {
  // First go through and add local 16p favicons.
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    TestFaviconData favicon = BuildFaviconData(i);
    TriggerSyncFaviconReceived(favicon.page_url, favicon.icon_url,
                               favicon.image_16, syncer::ProtoTimeToTime(i));
  }

  // Then go through and create the initial sync data, which does not have 16p
  // favicons for the first half, and has custom 16p favicons for the second.
  std::vector<syncer::SyncChange::SyncChangeType> expected_change_types;
  std::vector<int> expected_icons;
  std::vector<TestFaviconData> expected_data;
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    TestFaviconData test_data = BuildFaviconData(i);
    if (i < kFaviconBatchSize/2) {
      test_data.image_16 = std::string();
      expected_icons.push_back(i);
      expected_change_types.push_back(syncer::SyncChange::ACTION_UPDATE);
    } else {
      test_data.image_16 += "custom";
      expected_data.push_back(test_data);
    }
    FillImageSpecifics(test_data,
                       image_specifics.mutable_favicon_image());

    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    FillTrackingSpecifics(test_data,
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
  }

  syncer::SyncMergeResult merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_IMAGES,
                                        initial_image_data,
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize),
            cache()->GetAllSyncData(syncer::FAVICON_IMAGES).size());
  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize)/2, changes.size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_before_association());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_after_association());

  merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_TRACKING,
                                        initial_tracking_data,
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize),
            cache()->GetAllSyncData(syncer::FAVICON_TRACKING).size());
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_before_association());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_after_association());

  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
  ASSERT_TRUE(VerifyLocalCustomIcons(expected_data));
  ASSERT_TRUE(VerifyChanges(syncer::FAVICON_IMAGES,
                            expected_change_types,
                            expected_icons,
                            changes));
}

// Setting up sync with local data and sync data should merge the two tracking
// sets, such that the visit time is the most recent.
TEST_F(SyncFaviconCacheTest, SyncMergesTracking) {
  // First go through and add local 16p favicons.
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    TestFaviconData favicon = BuildFaviconData(i);
    TriggerSyncFaviconReceived(favicon.page_url, favicon.icon_url,
                               favicon.image_16, syncer::ProtoTimeToTime(i));
  }

  // Then go through and create the initial sync data, which for the first half
  // the local has a newer visit, and for the second the remote does.
  std::vector<syncer::SyncChange::SyncChangeType> expected_change_types;
  std::vector<int> expected_icons;
  std::vector<TestFaviconData> expected_data;
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    TestFaviconData test_data = BuildFaviconData(i);
    if (i < kFaviconBatchSize/2) {
      test_data.last_visit_time = syncer::ProtoTimeToTime(i - 1);
      expected_icons.push_back(i);
      expected_change_types.push_back(syncer::SyncChange::ACTION_UPDATE);
    } else {
      test_data.last_visit_time = syncer::ProtoTimeToTime(i + 1);
      expected_data.push_back(test_data);
    }
    FillImageSpecifics(test_data,
                       image_specifics.mutable_favicon_image());

    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    FillTrackingSpecifics(test_data,
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
  }

  syncer::SyncMergeResult merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_IMAGES,
                                        initial_image_data,
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize),
            cache()->GetAllSyncData(syncer::FAVICON_IMAGES).size());
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_before_association());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_after_association());

  merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_TRACKING,
                                        initial_tracking_data,
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize),
            cache()->GetAllSyncData(syncer::FAVICON_TRACKING).size());
  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize)/2, changes.size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_modified());
  EXPECT_EQ(0, merge_result.num_items_deleted());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_before_association());
  EXPECT_EQ(kFaviconBatchSize, merge_result.num_items_after_association());

  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
  ASSERT_TRUE(VerifyLocalCustomIcons(expected_data));
  ASSERT_TRUE(VerifyChanges(syncer::FAVICON_TRACKING,
                            expected_change_types,
                            expected_icons,
                            changes));
}

// Receiving old icons (missing image data) should result in pushing the new
// merged icons back to the remote syncer.
TEST_F(SyncFaviconCacheTest, ReceiveStaleImages) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList stale_changes;
  std::vector<int> expected_icons;
  std::vector<syncer::SyncChange::SyncChangeType> expected_change_types;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    expected_icons.push_back(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    expected_change_types.push_back(syncer::SyncChange::ACTION_UPDATE);
    image_specifics.mutable_favicon_image()->clear_favicon_web();
    stale_changes.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
        syncer::SyncData::CreateRemoteData(1, image_specifics)));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the same icons as an update, but with missing image data.
  cache()->ProcessSyncChanges(FROM_HERE, stale_changes);
  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
  ASSERT_EQ(static_cast<size_t>(kFaviconBatchSize), changes.size());
  ASSERT_TRUE(VerifyChanges(syncer::FAVICON_IMAGES,
                            expected_change_types,
                            expected_icons,
                            changes));
}

// New icons should be added locally without pushing anything back to the
// remote syncer.
TEST_F(SyncFaviconCacheTest, ReceiveNewImages) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList new_changes;
  std::vector<int> expected_icons;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    expected_icons.push_back(i);
    TestFaviconData test_data = BuildFaviconData(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(test_data,
                       image_specifics.mutable_favicon_image());
    new_changes.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
        syncer::SyncData::CreateRemoteData(1, image_specifics)));
    image_specifics.mutable_favicon_image()
        ->mutable_favicon_web()
        ->mutable_favicon()
        ->append("old");
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the new icons as an update.
  cache()->ProcessSyncChanges(FROM_HERE, new_changes);
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
}

// Recieving the same icons as the local data should have no effect.
TEST_F(SyncFaviconCacheTest, ReceiveSameImages) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList same_changes;
  std::vector<int> expected_icons;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    expected_icons.push_back(i);
    TestFaviconData test_data = BuildFaviconData(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(test_data,
                       image_specifics.mutable_favicon_image());
    same_changes.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
        syncer::SyncData::CreateRemoteData(1, image_specifics)));
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the new icons as an update.
  cache()->ProcessSyncChanges(FROM_HERE, same_changes);
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
}

// Receiving stale tracking (old visit times) should result in pushing back
// the newer visit times to the remote syncer.
TEST_F(SyncFaviconCacheTest, ReceiveStaleTracking) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList stale_changes;
  std::vector<int> expected_icons;
  std::vector<syncer::SyncChange::SyncChangeType> expected_change_types;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    expected_icons.push_back(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    expected_change_types.push_back(syncer::SyncChange::ACTION_UPDATE);
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
    tracking_specifics.mutable_favicon_tracking()->set_last_visit_time_ms(-1);
    stale_changes.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
        syncer::SyncData::CreateRemoteData(1, tracking_specifics)));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the same icons as an update, but with missing image data.
  cache()->ProcessSyncChanges(FROM_HERE, stale_changes);
  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
  ASSERT_EQ(static_cast<size_t>(kFaviconBatchSize), changes.size());
  ASSERT_TRUE(VerifyChanges(syncer::FAVICON_TRACKING,
                            expected_change_types,
                            expected_icons,
                            changes));
}

// New tracking information should be added locally without pushing anything
// back to the remote syncer.
TEST_F(SyncFaviconCacheTest, ReceiveNewTracking) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList new_changes;
  std::vector<int> expected_icons;
  // We start from one here so that we don't have to deal with a -1 visit time.
  for (int i = 1; i <= kFaviconBatchSize; ++i) {
    expected_icons.push_back(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    new_changes.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
        syncer::SyncData::CreateRemoteData(1, tracking_specifics)));
    tracking_specifics.mutable_favicon_tracking()->set_last_visit_time_ms(i-1);
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the new icons as an update.
  cache()->ProcessSyncChanges(FROM_HERE, new_changes);
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
}

// Receiving the same tracking information as the local data should have no
// effect.
TEST_F(SyncFaviconCacheTest, ReceiveSameTracking) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList same_changes;
  std::vector<int> expected_icons;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    expected_icons.push_back(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
    same_changes.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_UPDATE,
        syncer::SyncData::CreateRemoteData(1, tracking_specifics)));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the new icons as an update.
  cache()->ProcessSyncChanges(FROM_HERE, same_changes);
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  ASSERT_TRUE(VerifyLocalIcons(expected_icons));
}

// Verify we can delete favicons after setting up sync.
TEST_F(SyncFaviconCacheTest, DeleteFavicons) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList tracking_deletions, image_deletions;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
    tracking_deletions.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_DELETE,
        syncer::SyncData::CreateRemoteData(1, tracking_specifics)));
    image_deletions.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_DELETE,
        syncer::SyncData::CreateRemoteData(1, image_specifics)));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the tracking deletions. Since we'll still have orphan data,
  // the favicon count should remain the same.
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetFaviconCount());
  cache()->ProcessSyncChanges(FROM_HERE, tracking_deletions);
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetFaviconCount());

  // Once the image deletions arrive, the favicon count should be 0 again.
  cache()->ProcessSyncChanges(FROM_HERE, image_deletions);
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(0U, GetFaviconCount());
}

// Ensure that MergeDataAndStartSyncing enforces the sync favicon limit by
// dropping local icons.
TEST_F(SyncFaviconCacheTest, ExpireOnMergeData) {
  std::vector<int> expected_icons;
  syncer::SyncDataList initial_image_data, initial_tracking_data;

  // Set up sync so it has the maximum number of favicons, while the local has
  // the same amount of different favicons.
  for (int i = 0; i < kMaxSyncFavicons; ++i) {
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
    expected_icons.push_back(i);

    TestFaviconData favicon = BuildFaviconData(i+kMaxSyncFavicons);
    TriggerSyncFaviconReceived(favicon.page_url, favicon.icon_url,
                               favicon.image_16,
                               syncer::ProtoTimeToTime(i + kMaxSyncFavicons));
  }

  EXPECT_FALSE(VerifyLocalIcons(expected_icons));

  // Drops image part of the unsynced icons.
  syncer::SyncMergeResult merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_IMAGES,
                                        initial_image_data,
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ(static_cast<size_t>(kMaxSyncFavicons)*2,
            GetFaviconCount());  // Still have tracking.
  EXPECT_EQ(static_cast<size_t>(kMaxSyncFavicons),
            cache()->GetAllSyncData(syncer::FAVICON_IMAGES).size());
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(kMaxSyncFavicons, merge_result.num_items_added());
  EXPECT_EQ(0, merge_result.num_items_modified());
  EXPECT_EQ(kMaxSyncFavicons, merge_result.num_items_deleted());
  EXPECT_EQ(kMaxSyncFavicons, merge_result.num_items_before_association());
  EXPECT_EQ(kMaxSyncFavicons * 2, merge_result.num_items_after_association());

  // Drops tracking part of the unsynced icons.
  merge_result =
      cache()->MergeDataAndStartSyncing(syncer::FAVICON_TRACKING,
                                        initial_tracking_data,
                                        CreateAndPassProcessor(),
                                        CreateAndPassSyncErrorFactory());
  EXPECT_EQ(static_cast<size_t>(kMaxSyncFavicons),
            cache()->GetAllSyncData(syncer::FAVICON_TRACKING).size());
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(0, merge_result.num_items_added());
  EXPECT_EQ(kMaxSyncFavicons, merge_result.num_items_modified());
  EXPECT_EQ(kMaxSyncFavicons, merge_result.num_items_deleted());
  EXPECT_EQ(kMaxSyncFavicons * 2, merge_result.num_items_before_association());
  EXPECT_EQ(kMaxSyncFavicons, merge_result.num_items_after_association());

  EXPECT_TRUE(VerifyLocalIcons(expected_icons));
}

// Receiving sync additions (via ProcessSyncChanges) should not trigger
// expirations.
TEST_F(SyncFaviconCacheTest, NoExpireOnProcessSyncChanges) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  syncer::SyncChangeList image_changes, tracking_changes;
  std::vector<int> expected_icons;
  for (int i = 0; i < kMaxSyncFavicons; ++i) {
    expected_icons.push_back(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
    // Set up new tracking specifics for the icons received at change time.
    expected_icons.push_back(i + kMaxSyncFavicons);
    FillImageSpecifics(BuildFaviconData(i + kMaxSyncFavicons),
                       image_specifics.mutable_favicon_image());
    image_changes.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_ADD,
        syncer::SyncData::CreateRemoteData(1, image_specifics)));
    FillTrackingSpecifics(BuildFaviconData(i + kMaxSyncFavicons),
                          tracking_specifics.mutable_favicon_tracking());
    tracking_changes.push_back(syncer::SyncChange(
        FROM_HERE, syncer::SyncChange::ACTION_ADD,
        syncer::SyncData::CreateRemoteData(1, tracking_specifics)));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Now receive the new icons as an update.
  EXPECT_EQ(static_cast<size_t>(kMaxSyncFavicons), GetFaviconCount());
  cache()->ProcessSyncChanges(FROM_HERE, image_changes);
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  cache()->ProcessSyncChanges(FROM_HERE, tracking_changes);
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_TRUE(VerifyLocalIcons(expected_icons));
  EXPECT_LT(static_cast<size_t>(kMaxSyncFavicons), GetFaviconCount());
}

// Test that visiting a new page triggers a favicon load and a sync addition.
TEST_F(SyncFaviconCacheTest, AddOnFaviconVisited) {
  EXPECT_EQ(0U, GetFaviconCount());
  SetUpInitialSync(syncer::SyncDataList(), syncer::SyncDataList());
  std::vector<int> expected_icons;

  for (int i = 0; i < kFaviconBatchSize; ++i) {
    PopulateFaviconService(BuildFaviconData(i));
    expected_icons.push_back(i);
    TestFaviconData test_data = BuildFaviconData(i);
    cache()->OnFaviconVisited(test_data.page_url, test_data.icon_url);
  }

  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetTaskCount());
  // Wait until the FaviconService replies with the results populated above.
  RunUntilIdle();
  EXPECT_EQ(0U, GetTaskCount());
  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
  ASSERT_EQ(2U * kFaviconBatchSize, changes.size());

  for (int i = 0; i < kFaviconBatchSize; ++i) {
    TestFaviconData test_data = BuildFaviconData(i);
    const syncer::SyncChange& change1 = changes[i * 2];
    const syncer::SyncChange& change2 = changes[i * 2 + 1];

    EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change1.change_type());
    EXPECT_EQ(syncer::FAVICON_IMAGES, change1.sync_data().GetDataType());
    EXPECT_TRUE(CompareFaviconDataToSpecifics(
        test_data, change1.sync_data().GetSpecifics()));
    EXPECT_EQ(syncer::FAVICON_TRACKING, change2.sync_data().GetDataType());
    // Just verify the favicon url for the tracking specifics and that the
    // timestamp is non-null.
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD, change2.change_type());
    EXPECT_EQ(
        test_data.icon_url.spec(),
        change2.sync_data().GetSpecifics().favicon_tracking().favicon_url());
    EXPECT_NE(change2.sync_data()
                  .GetSpecifics()
                  .favicon_tracking()
                  .last_visit_time_ms(),
              0);
  }

  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetFaviconCount());
}

// Test that visiting a known page does not trigger a favicon load and just
// updates the sync tracking info.
TEST_F(SyncFaviconCacheTest, UpdateOnFaviconVisited) {
  EXPECT_EQ(0U, GetFaviconCount());
  SetUpInitialSync(syncer::SyncDataList(), syncer::SyncDataList());
  std::vector<int> expected_icons;

  // Add the favicons.
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    expected_icons.push_back(i);
    TestFaviconData test_data = BuildFaviconData(i);
    PopulateFaviconService(test_data);
    cache()->OnFaviconVisited(test_data.page_url, test_data.icon_url);
  }

  // Wait until the FaviconService replies with the results populated above.
  RunUntilIdle();
  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();

  // Visit the favicons again.
  EXPECT_EQ(0U, GetTaskCount());
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    TestFaviconData test_data = BuildFaviconData(i);
    cache()->OnFaviconVisited(test_data.page_url, test_data.icon_url);
    // Wait until the FaviconService replies with the results populated earlier.
    RunUntilIdle();

    syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
    ASSERT_EQ(1U, changes.size());
    // Just verify the favicon url for the tracking specifics and that the
    // timestamp is non-null.
    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, changes[0].change_type());
    EXPECT_EQ(test_data.icon_url.spec(),
              changes[0].sync_data().GetSpecifics().favicon_tracking().
                  favicon_url());
    EXPECT_NE(changes[0].sync_data().GetSpecifics().favicon_tracking().
                  last_visit_time_ms(), 0);
  }
  EXPECT_EQ(0U, GetTaskCount());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetFaviconCount());
}

// Ensure we properly expire old synced favicons as new ones are updated.
TEST_F(SyncFaviconCacheTest, ExpireOnFaviconVisited) {
  EXPECT_EQ(0U, GetFaviconCount());
  SetUpInitialSync(syncer::SyncDataList(), syncer::SyncDataList());
  std::vector<int> expected_icons;

  // Add the initial favicons.
  for (int i = 0; i < kMaxSyncFavicons; ++i) {
    expected_icons.push_back(i);
    TestFaviconData test_data = BuildFaviconData(i);
    PopulateFaviconService(test_data);
    cache()->OnFaviconVisited(test_data.page_url, test_data.icon_url);
  }

  // Wait until the FaviconService replies with the results populated above.
  RunUntilIdle();
  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();

  // Visit some new favicons, triggering expirations of the old favicons.
  EXPECT_EQ(0U, GetTaskCount());
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    TestFaviconData old_favicon = BuildFaviconData(i);
    TestFaviconData test_data = BuildFaviconData(i + kMaxSyncFavicons);
    PopulateFaviconService(test_data);
    cache()->OnFaviconVisited(test_data.page_url, test_data.icon_url);
    // Wait until the FaviconService replies with the results populated above.
    RunUntilIdle();

    syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
    ASSERT_EQ(4U, changes.size());
    EXPECT_EQ(syncer::SyncChange::ACTION_ADD, changes[0].change_type());
    EXPECT_TRUE(
        CompareFaviconDataToSpecifics(test_data,
                                      changes[0].sync_data().GetSpecifics()));
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, changes[1].change_type());
    EXPECT_EQ(old_favicon.icon_url.spec(),
              syncer::SyncDataLocal(changes[1].sync_data()).GetTag());

    EXPECT_EQ(syncer::SyncChange::ACTION_ADD, changes[2].change_type());
    EXPECT_EQ(test_data.icon_url.spec(),
              changes[2].sync_data().GetSpecifics().favicon_tracking().
                  favicon_url());
    EXPECT_NE(changes[2].sync_data().GetSpecifics().favicon_tracking().
                  last_visit_time_ms(), 0);
    EXPECT_EQ(syncer::SyncChange::ACTION_DELETE, changes[3].change_type());
    EXPECT_EQ(old_favicon.icon_url.spec(),
              syncer::SyncDataLocal(changes[3].sync_data()).GetTag());
  }

  EXPECT_EQ(0U, GetTaskCount());
  EXPECT_EQ(static_cast<size_t>(kMaxSyncFavicons), GetFaviconCount());
}

// A full history clear notification should result in all synced favicons being
// deleted.
TEST_F(SyncFaviconCacheTest, HistoryFullClear) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  std::vector<int> expected_icons;
  std::vector<syncer::SyncChange::SyncChangeType> expected_deletions;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    expected_icons.push_back(i);
    expected_deletions.push_back(syncer::SyncChange::ACTION_DELETE);
    TestFaviconData test_data = BuildFaviconData(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(test_data,
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);
  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
  EXPECT_TRUE(changes.empty());

  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetFaviconCount());
  cache()->OnURLsDeleted(nullptr, history::DeletionInfo::ForAllHistory());
  EXPECT_EQ(0U, GetFaviconCount());
  changes = processor()->GetAndResetChangeList();
  ASSERT_EQ(changes.size(), static_cast<size_t>(kFaviconBatchSize)*2);
  syncer::SyncChangeList changes_1, changes_2;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    changes_1.push_back(changes[i]);
    changes_2.push_back(changes[i + kFaviconBatchSize]);
  }
  VerifyChanges(syncer::FAVICON_IMAGES,
                expected_deletions,
                expected_icons,
                changes_1);
  VerifyChanges(syncer::FAVICON_TRACKING,
                expected_deletions,
                expected_icons,
                changes_2);
}

// A partial history clear notification should result in the expired favicons
// also being deleted from sync.
TEST_F(SyncFaviconCacheTest, HistorySubsetClear) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  std::vector<int> expected_icons;
  std::vector<syncer::SyncChange::SyncChangeType> expected_deletions;
  std::set<GURL> favicon_urls_to_delete;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    TestFaviconData test_data = BuildFaviconData(i);
    if (i < kFaviconBatchSize/2) {
      expected_icons.push_back(i);
      expected_deletions.push_back(syncer::SyncChange::ACTION_DELETE);
      favicon_urls_to_delete.insert(test_data.icon_url);
    }
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(test_data,
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);
  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
  EXPECT_TRUE(changes.empty());

  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetFaviconCount());
  cache()->OnURLsDeleted(
      nullptr, history::DeletionInfo::ForUrls(history::URLRows(),
                                              favicon_urls_to_delete));
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize)/2, GetFaviconCount());
  changes = processor()->GetAndResetChangeList();
  ASSERT_EQ(changes.size(), static_cast<size_t>(kFaviconBatchSize));
  syncer::SyncChangeList changes_1, changes_2;
  for (size_t i = 0; i < kFaviconBatchSize/2; ++i) {
    changes_1.push_back(changes[i]);
    changes_2.push_back(changes[i + kFaviconBatchSize/2]);
  }
  VerifyChanges(syncer::FAVICON_IMAGES,
                expected_deletions,
                expected_icons,
                changes_1);
  VerifyChanges(syncer::FAVICON_TRACKING,
                expected_deletions,
                expected_icons,
                changes_2);
}

// Any favicon urls with the "data" scheme should be ignored.
TEST_F(SyncFaviconCacheTest, IgnoreDataScheme) {
  EXPECT_EQ(0U, GetFaviconCount());
  SetUpInitialSync(syncer::SyncDataList(), syncer::SyncDataList());
  std::vector<int> expected_icons;

  for (int i = 0; i < kFaviconBatchSize; ++i) {
    TestFaviconData test_data = BuildFaviconData(i);
    test_data.icon_url = GURL("data:image/png;base64;blabla");
    EXPECT_TRUE(test_data.icon_url.is_valid());
    PopulateFaviconService(test_data);
    cache()->OnFaviconVisited(test_data.page_url, GURL());
  }

  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetTaskCount());
  // Wait until the FaviconService replies with the results populated above.
  RunUntilIdle();

  EXPECT_EQ(0U, GetTaskCount());
  EXPECT_EQ(0U, GetFaviconCount());
  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
  EXPECT_TRUE(changes.empty());
}

// When visiting a page we've already loaded the favicon for, don't attempt to
// reload the favicon, just update the visit time using the cached icon url.
TEST_F(SyncFaviconCacheTest, ReuseCachedIconUrl) {
  EXPECT_EQ(0U, GetFaviconCount());
  SetUpInitialSync(syncer::SyncDataList(), syncer::SyncDataList());
  std::vector<int> expected_icons;

  for (int i = 0; i < kFaviconBatchSize; ++i) {
    expected_icons.push_back(i);
    TestFaviconData test_data = BuildFaviconData(i);
    PopulateFaviconService(test_data);
    cache()->OnFaviconVisited(test_data.page_url, test_data.icon_url);
  }

  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetTaskCount());
  // Wait until the FaviconService replies with the results populated above.
  RunUntilIdle();

  processor()->GetAndResetChangeList();
  EXPECT_EQ(0U, GetTaskCount());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetFaviconCount());

  for (int i = 0; i < kFaviconBatchSize; ++i) {
    TestFaviconData test_data = BuildFaviconData(i);
    cache()->OnFaviconVisited(test_data.page_url, test_data.icon_url);
    // Wait until the FaviconService replies with the results populated earlier.
    RunUntilIdle();
    syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
    ASSERT_EQ(1U, changes.size());
    // Just verify the favicon url for the tracking specifics and that the
    // timestamp is non-null.
    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, changes[0].change_type());
    EXPECT_EQ(test_data.icon_url.spec(),
              changes[0].sync_data().GetSpecifics().favicon_tracking().
                  favicon_url());
    EXPECT_NE(changes[0].sync_data().GetSpecifics().favicon_tracking().
                  last_visit_time_ms(), 0);
  }
  EXPECT_EQ(0U, GetTaskCount());
}

// If we wind up with orphan image/tracking nodes, then receive an update
// for those favicons, we should lazily create the missing nodes.
TEST_F(SyncFaviconCacheTest, UpdatedOrphans) {
  EXPECT_EQ(0U, GetFaviconCount());
  SetUpInitialSync(syncer::SyncDataList(), syncer::SyncDataList());

  syncer::SyncChangeList initial_image_changes;
  syncer::SyncChangeList initial_tracking_changes;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    TestFaviconData test_data = BuildFaviconData(i);
    // Even favicons have image data but no tracking data. Odd favicons have
    // tracking data but no image data.
    if (i % 2 == 0) {
      sync_pb::EntitySpecifics image_specifics;
      FillImageSpecifics(BuildFaviconData(i),
                         image_specifics.mutable_favicon_image());
      initial_image_changes.push_back(syncer::SyncChange(
          FROM_HERE, syncer::SyncChange::ACTION_ADD,
          syncer::SyncData::CreateRemoteData(1, image_specifics)));
    } else {
      sync_pb::EntitySpecifics tracking_specifics;
      FillTrackingSpecifics(BuildFaviconData(i),
                            tracking_specifics.mutable_favicon_tracking());
      initial_tracking_changes.push_back(syncer::SyncChange(
          FROM_HERE, syncer::SyncChange::ACTION_ADD,
          syncer::SyncData::CreateRemoteData(1, tracking_specifics)));
    }
  }

  cache()->ProcessSyncChanges(FROM_HERE, initial_image_changes);
  cache()->ProcessSyncChanges(FROM_HERE, initial_tracking_changes);
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetFaviconCount());

  for (int i = 0; i < kFaviconBatchSize/2; ++i) {
    TestFaviconData test_data = BuildFaviconData(i);
    PopulateFaviconService(test_data);
    cache()->OnFaviconVisited(test_data.page_url, GURL());
    EXPECT_EQ(1U, GetTaskCount());
    // Wait until the FaviconService replies with the results populated above.
    RunUntilIdle();
    EXPECT_EQ(0U, GetTaskCount());
    syncer::SyncChangeList changes = processor()->GetAndResetChangeList();

    // Even favicons had image data, so should now receive new tracking data
    // and updated image data (we allow one update after the initial add).
    // Odd favicons had tracking so should now receive new image data and
    // updated tracking data.
    if (i % 2 == 0) {
      ASSERT_EQ(2U, changes.size());
      EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, changes[0].change_type());
      EXPECT_TRUE(
          CompareFaviconDataToSpecifics(test_data,
                                        changes[0].sync_data().GetSpecifics()));
      EXPECT_EQ(syncer::SyncChange::ACTION_ADD, changes[1].change_type());
      EXPECT_EQ(test_data.icon_url.spec(),
                changes[1].sync_data().GetSpecifics().favicon_tracking().
                    favicon_url());
      EXPECT_NE(changes[1].sync_data().GetSpecifics().favicon_tracking().
                    last_visit_time_ms(), 0);
    } else {
      ASSERT_EQ(2U, changes.size());
      EXPECT_EQ(syncer::SyncChange::ACTION_ADD, changes[0].change_type());
      EXPECT_TRUE(
          CompareFaviconDataToSpecifics(test_data,
                                        changes[0].sync_data().GetSpecifics()));
      EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, changes[1].change_type());
      EXPECT_EQ(test_data.icon_url.spec(),
                changes[1].sync_data().GetSpecifics().favicon_tracking().
                    favicon_url());
      EXPECT_NE(changes[1].sync_data().GetSpecifics().favicon_tracking().
                    last_visit_time_ms(), 0);
    }
  }

  EXPECT_EQ(0U, GetTaskCount());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetFaviconCount());
}

// Verify that orphaned favicon images don't result in creating invalid
// favicon tracking data.
TEST_F(SyncFaviconCacheTest, PartialAssociationInfo) {
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    sync_pb::EntitySpecifics image_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    image_specifics.mutable_favicon_image()->clear_favicon_web();
  }

  SetUpInitialSync(initial_image_data, initial_tracking_data);
  syncer::SyncChangeList change_list = processor()->GetAndResetChangeList();
  EXPECT_TRUE(change_list.empty());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetFaviconCount());
}

// Tests that we don't choke if a favicon visit node with a null visit time is
// present (see crbug.com/258196) and an update is made.
TEST_F(SyncFaviconCacheTest, NullFaviconVisitTime) {
  EXPECT_EQ(0U, GetFaviconCount());

  syncer::SyncDataList initial_image_data, initial_tracking_data;
  std::vector<int> expected_icons;
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    expected_icons.push_back(i);
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    FillImageSpecifics(BuildFaviconData(i),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    tracking_specifics.mutable_favicon_tracking()->set_last_visit_time_ms(
        syncer::TimeToProtoTime(base::Time()));
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
  }

  cache()->MergeDataAndStartSyncing(syncer::FAVICON_IMAGES,
                                    initial_image_data,
                                    CreateAndPassProcessor(),
                                    CreateAndPassSyncErrorFactory());
  ASSERT_EQ(0U, processor()->GetAndResetChangeList().size());
  cache()->MergeDataAndStartSyncing(syncer::FAVICON_TRACKING,
                                    initial_tracking_data,
                                    CreateAndPassProcessor(),
                                    CreateAndPassSyncErrorFactory());
  ASSERT_EQ(static_cast<size_t>(kFaviconBatchSize),
            processor()->GetAndResetChangeList().size());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetFaviconCount());

  // Visit the favicons again.
  EXPECT_EQ(0U, GetTaskCount());
  for (int i = 0; i < kFaviconBatchSize; ++i) {
    TestFaviconData test_data = BuildFaviconData(i);
    cache()->OnFaviconVisited(test_data.page_url, test_data.icon_url);
    // Wait until the FaviconService replies with the results populated earlier.
    RunUntilIdle();

    syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
    ASSERT_EQ(1U, changes.size());
    // Just verify the favicon url for the tracking specifics and that the
    // timestamp is non-null.
    EXPECT_EQ(syncer::SyncChange::ACTION_UPDATE, changes[0].change_type());
    EXPECT_EQ(test_data.icon_url.spec(),
              changes[0].sync_data().GetSpecifics().favicon_tracking().
                  favicon_url());
    EXPECT_NE(changes[0].sync_data().GetSpecifics().favicon_tracking().
                  last_visit_time_ms(), 0);
  }
  EXPECT_EQ(0U, GetTaskCount());
  EXPECT_EQ(static_cast<size_t>(kFaviconBatchSize), GetFaviconCount());
}

// If another synced client has a clock skewed towards the future, it's possible
// that favicons added locally will be expired as they are added. Ensure this
// doesn't crash (see crbug.com/306150).
TEST_F(SyncFaviconCacheTest, VisitFaviconClockSkew) {
  EXPECT_EQ(0U, GetFaviconCount());
  const int kClockSkew = 20;  // 20 minutes in the future.

  // Set up sync with kMaxSyncFavicons starting kClockSkew minutes in the
  // future.
  syncer::SyncDataList initial_image_data, initial_tracking_data;
  for (int i = 0; i < kMaxSyncFavicons; ++i) {
    sync_pb::EntitySpecifics image_specifics, tracking_specifics;
    TestFaviconData test_data = BuildFaviconData(i);
    test_data.last_visit_time =
        base::Time::Now() + base::TimeDelta::FromMinutes(kClockSkew);
    FillImageSpecifics(test_data,
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));
    FillTrackingSpecifics(test_data,
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
  }
  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // Visit some new favicons with local time, which will be expired as they
  // are added.
  EXPECT_EQ(0U, GetTaskCount());
  for (int i = 0; i < kClockSkew; ++i) {
    TestFaviconData test_data = BuildFaviconData(i + kMaxSyncFavicons);
    PopulateFaviconService(test_data);
    cache()->OnFaviconVisited(test_data.page_url, test_data.icon_url);
    // Wait until the FaviconService replies with the results populated above.
    RunUntilIdle();

    // The changes will be an add followed by a delete for both the image and
    // tracking info.
    syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
    ASSERT_EQ(changes.size(), 4U);
    ASSERT_EQ(changes[0].change_type(), syncer::SyncChange::ACTION_ADD);
    ASSERT_EQ(changes[0].sync_data().GetDataType(), syncer::FAVICON_IMAGES);
    ASSERT_EQ(changes[1].change_type(), syncer::SyncChange::ACTION_DELETE);
    ASSERT_EQ(changes[1].sync_data().GetDataType(), syncer::FAVICON_IMAGES);
    ASSERT_EQ(changes[2].change_type(), syncer::SyncChange::ACTION_ADD);
    ASSERT_EQ(changes[2].sync_data().GetDataType(), syncer::FAVICON_TRACKING);
    ASSERT_EQ(changes[3].change_type(), syncer::SyncChange::ACTION_DELETE);
    ASSERT_EQ(changes[3].sync_data().GetDataType(), syncer::FAVICON_TRACKING);
  }
  EXPECT_EQ(0U, GetTaskCount());
  EXPECT_EQ(static_cast<size_t>(kMaxSyncFavicons), GetFaviconCount());
}

// Simulate a case where the set of tracking info and image info doesn't match,
// and there is more tracking info than the max. A local update should correctly
// determine whether to update/add an image/tracking entity.
TEST_F(SyncFaviconCacheTest, MixedThreshold) {
  // First go through and add local favicons.
  for (int i = kMaxSyncFavicons; i < kMaxSyncFavicons + 5; ++i) {
    TestFaviconData favicon = BuildFaviconData(i);
    TriggerSyncFaviconReceived(favicon.page_url,
                               favicon.icon_url,
                               favicon.image_16,
                               favicon.last_visit_time);
  }

  syncer::SyncDataList initial_image_data, initial_tracking_data;
  // Then sync with enough favicons such that the tracking info is over the max
  // after merge completes.
  for (int i = 0; i < kMaxSyncFavicons; ++i) {
    sync_pb::EntitySpecifics image_specifics;
    // Push the images forward by 5, to match the unsynced favicons.
    FillImageSpecifics(BuildFaviconData(i + 5),
                       image_specifics.mutable_favicon_image());
    initial_image_data.push_back(
        syncer::SyncData::CreateRemoteData(1, image_specifics));

    sync_pb::EntitySpecifics tracking_specifics;
    FillTrackingSpecifics(BuildFaviconData(i),
                          tracking_specifics.mutable_favicon_tracking());
    initial_tracking_data.push_back(
        syncer::SyncData::CreateRemoteData(1, tracking_specifics));
  }
  SetUpInitialSync(initial_image_data, initial_tracking_data);

  // The local unsynced tracking info should be dropped, but not deleted.
  EXPECT_EQ(0U, processor()->GetAndResetChangeList().size());

  // Because the image and tracking data don't overlap, the total number of
  // favicons is still over the limit.
  EXPECT_EQ(static_cast<size_t>(kMaxSyncFavicons)+5, GetFaviconCount());

  // Trigger a tracking change for one of the favicons whose tracking info
  // was dropped, resulting in a tracking add and expiration of the orphaned
  // images.
  TestFaviconData test_data = BuildFaviconData(kMaxSyncFavicons);
  cache()->OnFaviconVisited(test_data.page_url, test_data.icon_url);

  syncer::SyncChangeList changes = processor()->GetAndResetChangeList();
  // 1 image update, 5 image deletions, 1 tracking deletion.
  ASSERT_EQ(6U, changes.size());
  // Expire image for favicon[kMaxSyncFavicons + 1].
  EXPECT_EQ(changes[0].change_type(), syncer::SyncChange::ACTION_DELETE);
  EXPECT_EQ(changes[0].sync_data().GetDataType(), syncer::FAVICON_IMAGES);
  EXPECT_EQ(kMaxSyncFavicons + 1, GetFaviconId(changes[0]));
  // Expire image for favicon[kMaxSyncFavicons + 2].
  EXPECT_EQ(changes[1].change_type(), syncer::SyncChange::ACTION_DELETE);
  EXPECT_EQ(changes[1].sync_data().GetDataType(), syncer::FAVICON_IMAGES);
  EXPECT_EQ(kMaxSyncFavicons + 2, GetFaviconId(changes[1]));
  // Expire image for favicon[kMaxSyncFavicons + 3].
  EXPECT_EQ(changes[2].change_type(), syncer::SyncChange::ACTION_DELETE);
  EXPECT_EQ(changes[2].sync_data().GetDataType(), syncer::FAVICON_IMAGES);
  EXPECT_EQ(kMaxSyncFavicons + 3, GetFaviconId(changes[2]));
  // Expire image for favicon[kMaxSyncFavicons + 4].
  EXPECT_EQ(changes[3].change_type(), syncer::SyncChange::ACTION_DELETE);
  EXPECT_EQ(changes[3].sync_data().GetDataType(), syncer::FAVICON_IMAGES);
  EXPECT_EQ(kMaxSyncFavicons + 4, GetFaviconId(changes[3]));
  // Update tracking for favicon[kMaxSyncFavicons].
  EXPECT_EQ(changes[4].change_type(), syncer::SyncChange::ACTION_ADD);
  EXPECT_EQ(changes[4].sync_data().GetDataType(), syncer::FAVICON_TRACKING);
  EXPECT_EQ(kMaxSyncFavicons, GetFaviconId(changes[4]));
  // Expire tracking for favicon[0].
  EXPECT_EQ(changes[5].change_type(), syncer::SyncChange::ACTION_DELETE);
  EXPECT_EQ(changes[5].sync_data().GetDataType(), syncer::FAVICON_TRACKING);
  EXPECT_EQ(0, GetFaviconId(changes[5]));
}

}  // namespace sync_sessions
