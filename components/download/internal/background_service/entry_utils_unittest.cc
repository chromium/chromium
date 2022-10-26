// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/entry_utils.h"

#include "base/ranges/algorithm.h"
#include "components/download/internal/background_service/test/entry_utils.h"
#include "components/download/internal/background_service/test/test_download_driver.h"
#include "components/download/public/background_service/clients.h"
#include "components/download/public/background_service/download_metadata.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {

namespace {
constexpr char kKey[] = "k";
constexpr char kValue[] = "v";
}  // namespace

TEST(DownloadServiceEntryUtilsTest, TestGetNumberOfLiveEntriesForClient) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  Entry entry3 = test::BuildBasicEntry();
  Entry entry4 = test::BuildBasicEntry(Entry::State::COMPLETE);

  std::vector<Entry*> entries = {&entry1, &entry2, &entry3, &entry4};

  EXPECT_EQ(0U, util::GetNumberOfLiveEntriesForClient(DownloadClient::INVALID,
                                                      entries));
  EXPECT_EQ(
      3U, util::GetNumberOfLiveEntriesForClient(DownloadClient::TEST, entries));
}

TEST(DownloadServiceEntryUtilsTest, MapEntriesToClients) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  Entry entry3 = test::BuildBasicEntry();
  Entry entry4 = test::BuildBasicEntry(Entry::State::COMPLETE);
  Entry entry5 = test::BuildBasicEntry(Entry::State::AVAILABLE);

  std::vector<Entry*> entries = {&entry1, &entry2, &entry3, &entry4, &entry5};
  test::TestDownloadDriver driver;
  std::vector<DownloadMetaData> expected_list = {
      util::BuildDownloadMetaData(&entry1, &driver),
      util::BuildDownloadMetaData(&entry2, &driver),
      util::BuildDownloadMetaData(&entry3, &driver),
      util::BuildDownloadMetaData(&entry4, &driver),
      util::BuildDownloadMetaData(&entry5, &driver)};
  std::vector<DownloadMetaData> expected_pruned_list = {
      util::BuildDownloadMetaData(&entry1, &driver),
      util::BuildDownloadMetaData(&entry2, &driver),
      util::BuildDownloadMetaData(&entry3, &driver),
      util::BuildDownloadMetaData(&entry5, &driver)};

  // If DownloadClient::TEST isn't a valid Client, all of the associated entries
  // should move to the DownloadClient::INVALID bucket.
  auto mapped1 = util::MapEntriesToMetadataForClients(
      std::set<DownloadClient>(), entries, &driver);
  EXPECT_EQ(1U, mapped1.size());
  EXPECT_NE(mapped1.end(), mapped1.find(DownloadClient::INVALID));
  EXPECT_EQ(mapped1.end(), mapped1.find(DownloadClient::TEST));

  auto list1 = mapped1.find(DownloadClient::INVALID)->second;
  EXPECT_TRUE(base::ranges::equal(expected_list, list1));

  // If DownloadClient::TEST is a valid Client, it should have the associated
  // entries.
  std::set<DownloadClient> clients = {DownloadClient::TEST};
  auto mapped2 =
      util::MapEntriesToMetadataForClients(clients, entries, &driver);
  EXPECT_EQ(1U, mapped2.size());
  EXPECT_NE(mapped2.end(), mapped2.find(DownloadClient::TEST));
  EXPECT_EQ(mapped2.end(), mapped2.find(DownloadClient::INVALID));

  auto list2 = mapped2.find(DownloadClient::TEST)->second;
  EXPECT_TRUE(base::ranges::equal(expected_list, list2));
}

TEST(DownloadServiceEntryUtilsTest, GetSchedulingCriteria) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  Entry entry3 = test::BuildBasicEntry();
  Entry entry4 = test::BuildBasicEntry();
  Entry entry5 = test::BuildBasicEntry();

  entry1.scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::UNMETERED;
  entry1.scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_CHARGING;

  entry2.scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::NONE;
  entry2.scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_CHARGING;

  entry3.scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::UNMETERED;
  entry3.scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;

  entry4.scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::NONE;
  entry4.scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;

  entry5.scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::UNMETERED;
  entry5.scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE;

  Model::EntryList list1 = {&entry1};
  Model::EntryList list2 = {&entry1, &entry2};
  Model::EntryList list3 = {&entry1, &entry3};
  Model::EntryList list4 = {&entry1, &entry2, &entry3};
  Model::EntryList list5 = {&entry1, &entry4};
  Model::EntryList list6 = {&entry3, &entry5};
  Model::EntryList list7 = {&entry5};

  const int kDownloadBatteryPercentage = 50;

  EXPECT_EQ(Criteria(true, true, kDownloadBatteryPercentage),
            util::GetSchedulingCriteria(list1, kDownloadBatteryPercentage));
  EXPECT_EQ(Criteria(true, false, kDownloadBatteryPercentage),
            util::GetSchedulingCriteria(list2, kDownloadBatteryPercentage));
  EXPECT_EQ(Criteria(false, true, DeviceStatus::kBatteryPercentageAlwaysStart),
            util::GetSchedulingCriteria(list3, kDownloadBatteryPercentage));
  EXPECT_EQ(Criteria(false, false, DeviceStatus::kBatteryPercentageAlwaysStart),
            util::GetSchedulingCriteria(list4, kDownloadBatteryPercentage));
  EXPECT_EQ(Criteria(false, false, DeviceStatus::kBatteryPercentageAlwaysStart),
            util::GetSchedulingCriteria(list5, kDownloadBatteryPercentage));
  EXPECT_EQ(Criteria(false, true, DeviceStatus::kBatteryPercentageAlwaysStart),
            util::GetSchedulingCriteria(list6, kDownloadBatteryPercentage));
  EXPECT_EQ(Criteria(false, true, kDownloadBatteryPercentage),
            util::GetSchedulingCriteria(list7, kDownloadBatteryPercentage));
}

// Test to verify download meta data is built correctly.
TEST(DownloadServiceEntryUtilsTest, BuildDownloadMetaData) {
  Entry entry = test::BuildBasicEntry(Entry::State::PAUSED);
  entry.target_file_path = base::FilePath::FromUTF8Unsafe("123");
  entry.bytes_downloaded = 200u;
  test::TestDownloadDriver driver;

  auto meta_data = util::BuildDownloadMetaData(&entry, &driver);
  EXPECT_EQ(entry.guid, meta_data.guid);

  // Incomplete downloads don't copy the following data.
  EXPECT_FALSE(meta_data.completion_info.has_value());

  // Current size is always pulled from driver, and we don't persist the current
  // size in each OnDownloadUpdated call in DownloadDriver.
  EXPECT_EQ(0u, meta_data.current_size);

  entry = test::BuildBasicEntry(Entry::State::COMPLETE);
  entry.target_file_path = base::FilePath::FromUTF8Unsafe("123");
  entry.bytes_downloaded = 100u;
  entry.custom_data = {{kKey, kValue}};
  meta_data = util::BuildDownloadMetaData(&entry, &driver);
  EXPECT_EQ(entry.guid, meta_data.guid);
  EXPECT_TRUE(meta_data.completion_info.has_value());
  EXPECT_EQ(entry.target_file_path, meta_data.completion_info->path);
  EXPECT_EQ(1u, meta_data.completion_info->custom_data.size());
  EXPECT_EQ(kValue, meta_data.completion_info->custom_data[kKey]);
  EXPECT_EQ(entry.bytes_downloaded,
            meta_data.completion_info->bytes_downloaded);
}

// Test to verify the current size is correctly built into the meta data.
TEST(DownloadServiceEntryUtilsTest, BuildDownloadMetaDataCurrentSize) {
  Entry entry = test::BuildBasicEntry(Entry::State::PAUSED);
  entry.target_file_path = base::FilePath::FromUTF8Unsafe("123");
  entry.bytes_downloaded = 0u;

  test::TestDownloadDriver driver;
  DriverEntry driver_entry;
  driver_entry.guid = entry.guid;
  driver_entry.bytes_downloaded = 100u;
  driver.AddTestData({driver_entry});

  auto meta_data = util::BuildDownloadMetaData(&entry, &driver);
  meta_data = util::BuildDownloadMetaData(&entry, &driver);
  EXPECT_EQ(driver_entry.bytes_downloaded, meta_data.current_size);
}

}  // namespace download
