// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"

#include "base/sequenced_task_runner.h"
#include "components/send_tab_to_self/target_device_info.h"

namespace send_tab_to_self {

FakeSendTabToSelfModel::FakeSendTabToSelfModel() = default;
FakeSendTabToSelfModel::~FakeSendTabToSelfModel() = default;

std::vector<std::string> FakeSendTabToSelfModel::GetAllGuids() const {
  return {};
}

void FakeSendTabToSelfModel::DeleteAllEntries() {}

const SendTabToSelfEntry* FakeSendTabToSelfModel::GetEntryByGUID(
    const std::string& guid) const {
  return nullptr;
}

const SendTabToSelfEntry* FakeSendTabToSelfModel::AddEntry(
    const GURL& url,
    const std::string& title,
    base::Time navigation_time,
    const std::string& target_device_cache_guid) {
  return nullptr;
}

void FakeSendTabToSelfModel::DeleteEntry(const std::string& guid) {}
void FakeSendTabToSelfModel::DismissEntry(const std::string& guid) {}
void FakeSendTabToSelfModel::MarkEntryOpened(const std::string& guid) {}

bool FakeSendTabToSelfModel::IsReady() {
  return true;
}

bool FakeSendTabToSelfModel::HasValidTargetDevice() {
  return true;
}

std::vector<TargetDeviceInfo>
FakeSendTabToSelfModel::GetTargetDeviceInfoSortedList() {
  return {
      {"Fake Desktop 1 Long", "Fake Desktop 1", "D0000",
       sync_pb::SyncEnums_DeviceType_TYPE_LINUX, base::Time::Now()},
      {"Fake Desktop 2 Long", "Fake Desktop 2", "D0001",
       sync_pb::SyncEnums_DeviceType_TYPE_WIN,
       base::Time::Now() - base::Days(1)},
      {"Fake Phone Long", "Fake Phone", "D0002",
       sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
       base::Time::Now() - base::Days(7)},
  };
}

}  // namespace send_tab_to_self
