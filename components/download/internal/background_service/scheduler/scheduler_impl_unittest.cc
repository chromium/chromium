// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/background_service/scheduler/scheduler_impl.h"

#include <stdint.h>
#include <memory>

#include "base/strings/string_number_conversions.h"
#include "components/download/internal/background_service/config.h"
#include "components/download/internal/background_service/entry.h"
#include "components/download/internal/background_service/scheduler/device_status.h"
#include "components/download/public/task/task_scheduler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::InSequence;

namespace download {
namespace {

class MockTaskScheduler : public TaskScheduler {
 public:
  MockTaskScheduler() = default;
  ~MockTaskScheduler() override = default;

  MOCK_METHOD6(ScheduleTask,
               void(DownloadTaskType, bool, bool, int, int64_t, int64_t));
  MOCK_METHOD1(CancelTask, void(DownloadTaskType));
};

class DownloadSchedulerImplTest : public testing::Test {
 public:
  DownloadSchedulerImplTest() = default;

  DownloadSchedulerImplTest(const DownloadSchedulerImplTest&) = delete;
  DownloadSchedulerImplTest& operator=(const DownloadSchedulerImplTest&) =
      delete;

  ~DownloadSchedulerImplTest() override = default;

  void TearDown() override { DestroyScheduler(); }

  void BuildScheduler(const std::vector<DownloadClient> clients) {
    scheduler_ =
        std::make_unique<SchedulerImpl>(&task_scheduler_, &config_, clients);
  }
  void DestroyScheduler() { scheduler_.reset(); }

  // Helper function to create a list of entries for the scheduler to query the
  // next entry.
  void BuildDataEntries(size_t size) {
    entries_ = std::vector<Entry>(size, Entry());
    for (size_t i = 0; i < size; ++i) {
      entries_[i].guid = base::NumberToString(i);
      entries_[i].scheduling_params.battery_requirements =
          SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE;
      entries_[i].scheduling_params.network_requirements =
          SchedulingParams::NetworkRequirements::UNMETERED;
      entries_[i].state = Entry::State::AVAILABLE;
    }
  }

  // Returns list of entry pointers to feed to the scheduler.
  Model::EntryList entries() {
    Model::EntryList entry_list;
    for (auto& entry : entries_) {
      entry_list.emplace_back(&entry);
    }
    return entry_list;
  }

  // Simulates the entry has been processed by the download service and the
  // state has changed.
  void MakeEntryActive(Entry* entry) {
    if (entry)
      entry->state = Entry::State::ACTIVE;
  }

  // Reverts the states of entry so that the scheduler can poll it again.
  void MakeEntryAvailable(Entry* entry) {
    entry->state = Entry::State::AVAILABLE;
  }

  // Helper function to build a device status.
  DeviceStatus BuildDeviceStatus(BatteryStatus battery, NetworkStatus network) {
    DeviceStatus device_status;
    device_status.battery_status = battery;
    device_status.network_status = network;
    return device_status;
  }

 protected:
  std::unique_ptr<SchedulerImpl> scheduler_;
  MockTaskScheduler task_scheduler_;
  Configuration config_;

  // Entries owned by the test fixture.
  std::vector<Entry> entries_;
};

// Ensures normal polling logic is correct.
TEST_F(DownloadSchedulerImplTest, BasicPolling) {
  BuildScheduler(std::vector<DownloadClient>{DownloadClient::TEST_2,
                                             DownloadClient::TEST});

  // Client TEST: entry 0.
  // Client TEST_2: entry 1.
  // Poll sequence: 1 -> 0.
  BuildDataEntries(2);
  entries_[0].client = DownloadClient::TEST;
  entries_[1].client = DownloadClient::TEST_2;

  // First download belongs to first client.
  Entry* next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(next, &entries_[1]);
  MakeEntryActive(next);

  // If the first one is processed, the next should be the other entry.
  next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(next, &entries_[0]);
  MakeEntryActive(next);
}

// Tests the load balancing and polling downloads based on cancel time.
TEST_F(DownloadSchedulerImplTest, BasicLoadBalancing) {
  BuildScheduler(std::vector<DownloadClient>{
      DownloadClient::TEST, DownloadClient::TEST_2, DownloadClient::TEST_3});

  // Client TEST: entry 0, entry 1 (earlier cancel time).
  // Client TEST_2: entry 2.
  // Client TEST_3: No entries.
  // Poll sequence: 1 -> 2 -> 0.
  BuildDataEntries(3);
  entries_[0].client = DownloadClient::TEST;
  entries_[0].scheduling_params.cancel_time = base::Time::FromInternalValue(20);
  entries_[1].client = DownloadClient::TEST;
  entries_[1].scheduling_params.cancel_time = base::Time::FromInternalValue(10);
  entries_[2].client = DownloadClient::TEST_2;
  entries_[2].scheduling_params.cancel_time = base::Time::FromInternalValue(30);

  // There are 2 downloads for client 0, the one with earlier create time will
  // be the next download.
  Entry* next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(&entries_[1], next);
  MakeEntryActive(next);

  // The second download should belongs to client 1, because of the round robin
  // load balancing.
  next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(&entries_[2], next);
  MakeEntryActive(next);

  // Only one entry left, which will be the next.
  next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(&entries_[0], next);
  MakeEntryActive(next);

  // Keep polling twice, since no available downloads, both will return nullptr.
  next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(nullptr, next);
  MakeEntryActive(next);

  next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(nullptr, next);
  MakeEntryActive(next);
}

// Ensures downloads are polled based on scheduling parameters and device
// status.
TEST_F(DownloadSchedulerImplTest, SchedulingParams) {
  BuildScheduler(std::vector<DownloadClient>{DownloadClient::TEST});
  BuildDataEntries(1);
  entries_[0].client = DownloadClient::TEST;
  entries_[0].scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE;
  entries_[0].scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::UNMETERED;

  Entry* next = nullptr;

  // Tests network scheduling parameter.
  // No downloads can be polled when network disconnected.
  next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::DISCONNECTED));
  EXPECT_EQ(nullptr, next);

  // If the network is metered, and scheduling parameter requires unmetered
  // network, the download should not be polled.
  next = scheduler_->Next(entries(), BuildDeviceStatus(BatteryStatus::CHARGING,
                                                       NetworkStatus::METERED));
  EXPECT_EQ(nullptr, next);

  // If the network requirement is none, the download can happen under metered
  // network. However, download won't happen when network is disconnected.
  entries_[0].scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::NONE;
  next = scheduler_->Next(entries(), BuildDeviceStatus(BatteryStatus::CHARGING,
                                                       NetworkStatus::METERED));
  EXPECT_EQ(&entries_[0], next);
  next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::DISCONNECTED));
  EXPECT_EQ(nullptr, next);
  MakeEntryActive(next);

  // Tests battery sensitive scheduling parameter.
  entries_[0].scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE;

  next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(&entries_[0], next);

  // Battery sensitive with low battery level.
  DeviceStatus status =
      BuildDeviceStatus(BatteryStatus::NOT_CHARGING, NetworkStatus::UNMETERED);
  DCHECK_EQ(status.battery_percentage, 0);
  next = scheduler_->Next(entries(), status);
  EXPECT_EQ(nullptr, next);

  status.battery_percentage = config_.download_battery_percentage - 1;
  next = scheduler_->Next(entries(), status);
  EXPECT_EQ(nullptr, next);

  // Battery sensitive with high battery level.
  status.battery_percentage = config_.download_battery_percentage;
  next = scheduler_->Next(entries(), status);
  EXPECT_EQ(&entries_[0], next);

  // Tests battery insensitive scheduling parameter.
  entries_[0].scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;
  next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::NOT_CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(&entries_[0], next);
  next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(&entries_[0], next);
  MakeEntryActive(next);
}

// Ensures higher priority will be scheduled first.
TEST_F(DownloadSchedulerImplTest, Priority) {
  BuildScheduler(std::vector<DownloadClient>{DownloadClient::TEST});

  // The second entry has higher priority but is created later than the first
  // entry. This ensures priority is checked before the create time.
  BuildDataEntries(2);
  entries_[0].client = DownloadClient::TEST;
  entries_[0].scheduling_params.priority = SchedulingParams::Priority::LOW;
  entries_[0].scheduling_params.cancel_time = base::Time::FromInternalValue(20);
  entries_[1].client = DownloadClient::TEST;
  entries_[1].scheduling_params.priority = SchedulingParams::Priority::HIGH;
  entries_[1].scheduling_params.cancel_time = base::Time::FromInternalValue(40);

  // Download with higher priority should be polled first, even if there is
  // another download created earlier.
  Entry* next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(&entries_[1], next);

  // Download with non UI priority should be subject to network and battery
  // scheduling parameters. The higher priority one will be ignored because of
  // mismatching battery condition.
  entries_[1].scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE;
  entries_[0].scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;

  next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::NOT_CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(&entries_[0], next);
  MakeEntryActive(next);
}

// Ensures UI priority entries are subject to device status check.
TEST_F(DownloadSchedulerImplTest, UIPrioritySubjectToDeviceStatus) {
  BuildScheduler(std::vector<DownloadClient>{DownloadClient::TEST,
                                             DownloadClient::TEST_2});

  // Client TEST: entry 0.
  // Client TEST_2: entry 1 (UI priority, cancel later).
  BuildDataEntries(2);
  entries_[0].client = DownloadClient::TEST;
  entries_[0].scheduling_params.priority = SchedulingParams::Priority::LOW;
  entries_[1].client = DownloadClient::TEST_2;
  entries_[1].scheduling_params.priority = SchedulingParams::Priority::UI;

  // UI priority is also subject to device status validation.
  Entry* next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::NOT_CHARGING, NetworkStatus::METERED));
  EXPECT_EQ(nullptr, next);
  MakeEntryActive(next);
}

// UI priority entries will be processed first even if they doesn't belong to
// the current client in load balancing.
TEST_F(DownloadSchedulerImplTest, UIPriorityLoadBalancing) {
  BuildScheduler(std::vector<DownloadClient>{DownloadClient::TEST,
                                             DownloadClient::TEST_2});

  // Client TEST: entry 0(Low priority).
  // Client TEST_2: entry 1(UI priority).
  BuildDataEntries(2);
  entries_[0].client = DownloadClient::TEST;
  entries_[0].scheduling_params.priority = SchedulingParams::Priority::LOW;
  entries_[1].client = DownloadClient::TEST_2;
  entries_[1].scheduling_params.priority = SchedulingParams::Priority::UI;

  Entry* next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(&entries_[1], next);
  MakeEntryActive(next);
}

TEST_F(DownloadSchedulerImplTest, PickOlderDownloadIfSameParameters) {
  BuildScheduler(std::vector<DownloadClient>{DownloadClient::TEST,
                                             DownloadClient::TEST_2});

  // Client TEST: entry 0(Low priority, No Cancel Time, Newer).
  // Client TEST: entry 1(Low priority, No Cancel Time, Older).
  // Client TEST: entry 2(Low priority, No Cancel Time, Newer).
  BuildDataEntries(3);
  entries_[0].client = DownloadClient::TEST;
  entries_[0].scheduling_params.priority = SchedulingParams::Priority::LOW;
  entries_[0].create_time = base::Time::Now();
  entries_[1].client = DownloadClient::TEST;
  entries_[1].scheduling_params.priority = SchedulingParams::Priority::LOW;
  entries_[1].create_time = base::Time::Now() - base::Days(1);
  entries_[2].client = DownloadClient::TEST;
  entries_[2].scheduling_params.priority = SchedulingParams::Priority::LOW;
  entries_[2].create_time = base::Time::Now();

  Entry* next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(&entries_[1], next);
  MakeEntryActive(next);
}

// When multiple UI priority entries exist, the next entry is selected based on
// cancel time and load balancing.
TEST_F(DownloadSchedulerImplTest, MultipleUIPriorityEntries) {
  BuildScheduler(std::vector<DownloadClient>{DownloadClient::TEST,
                                             DownloadClient::TEST_2});
  BuildDataEntries(4);

  // Client TEST: entry 0(UI priority), entry 1(UI priority, early cancel time).
  // Client TEST_2: entry 2(UI priority), entry 3(high priority, early cancel
  // time). Poll sequence: 1 -> 2 -> 0 -> 3.
  for (auto& entry : entries_) {
    entry.scheduling_params.priority = SchedulingParams::Priority::UI;
  }
  entries_[0].client = DownloadClient::TEST;
  entries_[0].scheduling_params.cancel_time = base::Time::FromInternalValue(40);
  entries_[1].client = DownloadClient::TEST;
  entries_[1].scheduling_params.cancel_time = base::Time::FromInternalValue(20);
  entries_[2].client = DownloadClient::TEST_2;
  entries_[2].scheduling_params.cancel_time = base::Time::FromInternalValue(50);
  entries_[3].client = DownloadClient::TEST_2;
  entries_[3].scheduling_params.cancel_time = base::Time::FromInternalValue(20);
  entries_[3].scheduling_params.priority = SchedulingParams::Priority::HIGH;

  // When device conditions are meet, UI priority entry with the earliest cancel
  // time will be processed first.
  Entry* next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(&entries_[1], next);
  MakeEntryActive(next);

  // Next entry will be UI priority entry from another client.
  next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(&entries_[2], next);
  MakeEntryActive(next);

  next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(&entries_[0], next);
  MakeEntryActive(next);

  next = scheduler_->Next(
      entries(),
      BuildDeviceStatus(BatteryStatus::CHARGING, NetworkStatus::UNMETERED));
  EXPECT_EQ(&entries_[3], next);
  MakeEntryActive(next);
}

// Ensures the reschedule logic works correctly, and we can pass the correct
// criteria to platform task scheduler.
TEST_F(DownloadSchedulerImplTest, Reschedule) {
  InSequence s;

  BuildScheduler(std::vector<DownloadClient>{DownloadClient::TEST});
  BuildDataEntries(2);
  entries_[0].client = DownloadClient::TEST;
  entries_[1].client = DownloadClient::TEST;

  entries_[0].scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_CHARGING;
  entries_[0].scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::UNMETERED;
  entries_[1].scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_CHARGING;
  entries_[1].scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::UNMETERED;

  Criteria criteria(config_.download_battery_percentage);
  EXPECT_CALL(task_scheduler_, CancelTask(DownloadTaskType::DOWNLOAD_TASK))
      .Times(0);
  EXPECT_CALL(task_scheduler_,
              ScheduleTask(DownloadTaskType::DOWNLOAD_TASK,
                           criteria.requires_unmetered_network,
                           criteria.requires_battery_charging, _, _, _))
      .RetiresOnSaturation();
  scheduler_->Reschedule(entries());

  entries_[0].scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;
  criteria.requires_battery_charging = false;
  EXPECT_CALL(task_scheduler_, CancelTask(DownloadTaskType::DOWNLOAD_TASK))
      .Times(0);
  EXPECT_CALL(task_scheduler_,
              ScheduleTask(DownloadTaskType::DOWNLOAD_TASK,
                           criteria.requires_unmetered_network,
                           criteria.requires_battery_charging, _, _, _))
      .RetiresOnSaturation();
  scheduler_->Reschedule(entries());

  entries_[0].scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::NONE;
  criteria.requires_unmetered_network = false;
  EXPECT_CALL(task_scheduler_, CancelTask(DownloadTaskType::DOWNLOAD_TASK))
      .Times(0);
  EXPECT_CALL(task_scheduler_,
              ScheduleTask(DownloadTaskType::DOWNLOAD_TASK,
                           criteria.requires_unmetered_network,
                           criteria.requires_battery_charging, _, _, _))
      .RetiresOnSaturation();
  scheduler_->Reschedule(entries());

  EXPECT_CALL(task_scheduler_, CancelTask(DownloadTaskType::DOWNLOAD_TASK))
      .RetiresOnSaturation();
  scheduler_->Reschedule(Model::EntryList());
}

}  // namespace
}  // namespace download
