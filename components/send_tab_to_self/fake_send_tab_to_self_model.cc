// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/send_tab_to_self/page_context.h"
#include "components/send_tab_to_self/send_tab_to_self_entry.h"
#include "components/send_tab_to_self/send_tab_to_self_model_observer.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "url/gurl.h"

namespace send_tab_to_self {

FakeSendTabToSelfModel::FakeSendTabToSelfModel() = default;
FakeSendTabToSelfModel::~FakeSendTabToSelfModel() = default;

std::vector<std::string> FakeSendTabToSelfModel::GetAllGuids() const {
  return base::ToVector(
      entries_, [](const std::pair<const std::string,
                                   std::unique_ptr<SendTabToSelfEntry>>& it) {
        return it.first;
      });
}

const SendTabToSelfEntry* FakeSendTabToSelfModel::GetEntryByGUID(
    const std::string& guid) const {
  std::map<std::string, std::unique_ptr<SendTabToSelfEntry>>::const_iterator
      it = entries_.find(guid);
  return it != entries_.end() ? it->second.get() : nullptr;
}

const SendTabToSelfEntry* FakeSendTabToSelfModel::SendEntry(
    const GURL& url,
    const std::string& title,
    const std::string& target_device_cache_guid,
    const PageContext& context,
    NavigationHistory navigation_history,
    base::OnceCallback<void(SendTabToSelfResult)> commit_confirmation) {
  if (!IsReady()) {
    if (commit_confirmation) {
      std::move(commit_confirmation)
          .Run(SendTabToSelfResult::kFailureNotTrackingMetadata);
    }
    return nullptr;
  }

  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::unique_ptr<SendTabToSelfEntry> entry =
      std::make_unique<SendTabToSelfEntry>(
          guid, url, title, base::Time::Now(), local_device_name_,
          target_device_cache_guid, context, std::move(navigation_history));

  const SendTabToSelfEntry* result = entry.get();
  entries_[guid] = std::move(entry);

  if (send_entry_callback_) {
    send_entry_callback_.Run(result);
  }

  for (auto& observer : observers_) {
    observer.EntryAddedLocally(result);
  }

  if (commit_confirmation) {
    std::move(commit_confirmation).Run(send_result_);
  }

  return result;
}

void FakeSendTabToSelfModel::DismissEntry(const std::string& guid) {
  last_dismissed_guid_ = guid;
  std::map<std::string, std::unique_ptr<SendTabToSelfEntry>>::iterator it =
      entries_.find(guid);
  if (it != entries_.end()) {
    it->second->SetNotificationDismissed(true);
  }
}

void FakeSendTabToSelfModel::MarkEntryOpened(const std::string& guid) {
  last_opened_guid_ = guid;
  std::map<std::string, std::unique_ptr<SendTabToSelfEntry>>::iterator it =
      entries_.find(guid);
  if (it != entries_.end()) {
    it->second->MarkOpened(base::Time::Now());
    for (auto& observer : observers_) {
      observer.EntriesOpenedRemotely({it->second.get()});
    }
  }
}

bool FakeSendTabToSelfModel::IsReady() {
  return is_ready_;
}

bool FakeSendTabToSelfModel::HasValidTargetDevice() {
  return has_valid_target_device_;
}

std::vector<TargetDeviceInfo>
FakeSendTabToSelfModel::GetTargetDeviceInfoSortedList() {
  return devices_;
}

void FakeSendTabToSelfModel::SetIsReady(bool is_ready) {
  is_ready_ = is_ready;
}

void FakeSendTabToSelfModel::SetHasValidTargetDevice(
    bool has_valid_target_device) {
  has_valid_target_device_ = has_valid_target_device;
}

void FakeSendTabToSelfModel::SetTargetDeviceInfoSortedList(
    const std::vector<TargetDeviceInfo>& devices) {
  devices_ = devices;
}

void FakeSendTabToSelfModel::AddTargetDevice(const TargetDeviceInfo& device) {
  devices_.push_back(device);
}

void FakeSendTabToSelfModel::SetLocalDeviceName(std::string_view device_name) {
  local_device_name_ = std::string(device_name);
}

void FakeSendTabToSelfModel::SetSendResult(SendTabToSelfResult result) {
  send_result_ = result;
}

void FakeSendTabToSelfModel::SetSendEntryCallback(SendEntryCallback callback) {
  send_entry_callback_ = std::move(callback);
}

const SendTabToSelfEntry* FakeSendTabToSelfModel::AddEntryRemotely(
    const GURL& url,
    const std::string& title,
    const std::string& target_device_cache_guid,
    const PageContext& context,
    NavigationHistory navigation_history) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::unique_ptr<SendTabToSelfEntry> entry =
      std::make_unique<SendTabToSelfEntry>(
          guid, url, title, base::Time::Now(), "remote_device",
          target_device_cache_guid, context, std::move(navigation_history));

  const SendTabToSelfEntry* result = entry.get();
  entries_[guid] = std::move(entry);

  for (auto& observer : observers_) {
    observer.EntriesAddedRemotely({result});
  }

  return result;
}

}  // namespace send_tab_to_self
