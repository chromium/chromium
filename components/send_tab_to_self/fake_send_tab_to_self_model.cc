// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"

#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/auto_reset.h"
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
    std::string_view guid) const {
  auto it = entries_.find(guid);
  if (it != entries_.end()) {
    return it->second.get();
  }
  for (const auto& entry : remote_entries_pending_model_ready_) {
    if (entry->GetGUID() == guid) {
      return entry.get();
    }
  }
  return nullptr;
}

std::vector<const SendTabToSelfEntry*>
FakeSendTabToSelfModel::GetUnopenedEntriesTargetedToLocalDevice() const {
  std::vector<const SendTabToSelfEntry*> unopened_entries;
  for (const auto& [guid, entry] : entries_) {
    if (entry->GetTargetDeviceSyncCacheGuid() == local_cache_guid_ &&
        !entry->IsOpened()) {
      unopened_entries.push_back(entry.get());
    }
  }
  std::ranges::sort(unopened_entries, {}, &SendTabToSelfEntry::GetSharedTime);
  return unopened_entries;
}

std::vector<const SendTabToSelfEntry*>
FakeSendTabToSelfModel::GetOpenedEntriesTargetedToLocalDevice() const {
  std::vector<const SendTabToSelfEntry*> opened_entries;
  for (const auto& [guid, entry] : entries_) {
    if (entry->GetTargetDeviceSyncCacheGuid() == local_cache_guid_ &&
        entry->IsOpened()) {
      opened_entries.push_back(entry.get());
    }
  }
  return opened_entries;
}

const SendTabToSelfEntry* FakeSendTabToSelfModel::SendEntry(
    const GURL& url,
    const std::string& title,
    const std::string& target_device_cache_guid,
    const PageContext& context,
    NavigationHistory navigation_history,
    base::OnceCallback<void(SendTabToSelfResult)> commit_confirmation,
    ShareEntryPoint entry_point) {
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
    observer.OnEntryAddedLocally(result);
  }

  if (commit_confirmation) {
    std::move(commit_confirmation).Run(send_result_);
  }

  return result;
}

void FakeSendTabToSelfModel::DismissEntry(std::string_view guid) {
  last_dismissed_guid_ = std::string(guid);
  auto it = entries_.find(guid);
  if (it != entries_.end()) {
    it->second->SetNotificationDismissed(true);
  }
}

void FakeSendTabToSelfModel::MarkEntryOpened(std::string_view guid) {
  last_opened_guid_ = std::string(guid);
  auto it = entries_.find(guid);
  if (it != entries_.end()) {
    it->second->MarkOpened(base::Time::Now());
  }
}

void FakeSendTabToSelfModel::MarkEntryActivated(
    std::string_view guid,
    ShareActivatedEntryPoint entry_point) {
  last_activated_guid_ = std::string(guid);
  last_activated_entry_point_ = entry_point;
  ++activated_call_count_;
  auto it = entries_.find(guid);
  if (it != entries_.end()) {
    it->second->MarkActivated(base::Time::Now());
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

std::optional<TargetDeviceInfo> FakeSendTabToSelfModel::GetTargetDeviceInfo(
    std::string_view cache_guid) {
  auto it =
      std::ranges::find(devices_, cache_guid, &TargetDeviceInfo::cache_guid);
  return it != devices_.end() ? std::make_optional(*it) : std::nullopt;
}

void FakeSendTabToSelfModel::SetIsReady(bool is_ready) {
  is_ready_ = is_ready;
  if (is_ready_) {
    if (!remote_entries_pending_model_ready_.empty()) {
      for (auto& entry : remote_entries_pending_model_ready_) {
        const std::string& guid = entry->GetGUID();
        entries_[guid] = std::move(entry);
      }
      remote_entries_pending_model_ready_.clear();
    }

    for (auto& observer : observers_) {
      observer.OnModelReady();
    }
  }
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

void FakeSendTabToSelfModel::SetLocalCacheGuid(std::string_view cache_guid) {
  local_cache_guid_ = std::string(cache_guid);
}

void FakeSendTabToSelfModel::SetSendResult(SendTabToSelfResult result) {
  send_result_ = result;
}

void FakeSendTabToSelfModel::SetSendEntryCallback(SendEntryCallback callback) {
  send_entry_callback_ = std::move(callback);
}

const SendTabToSelfEntry* FakeSendTabToSelfModel::AddEntryRemotely(
    RemoteEntryParams params) {
  return AddEntriesRemotely(std::vector<RemoteEntryParams>{std::move(params)})
      .front();
}

const SendTabToSelfEntry* FakeSendTabToSelfModel::AddEntryRemotely(
    const GURL& url,
    const std::string& title,
    const std::string& target_device_cache_guid,
    const PageContext& context,
    NavigationHistory navigation_history) {
  return AddEntryRemotely(
      {.url = url,
       .title = title,
       .target_device_cache_guid = target_device_cache_guid,
       .context = context,
       .navigation_history = std::move(navigation_history)});
}

std::vector<const SendTabToSelfEntry*>
FakeSendTabToSelfModel::AddEntriesRemotely(
    std::vector<RemoteEntryParams> entries_params) {
  std::vector<const SendTabToSelfEntry*> results;
  for (auto& params : entries_params) {
    std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
    base::Time entry_time =
        params.shared_time.is_null() ? base::Time::Now() : params.shared_time;
    std::unique_ptr<SendTabToSelfEntry> entry =
        std::make_unique<SendTabToSelfEntry>(
            guid, params.url, params.title, entry_time, "remote_device",
            params.target_device_cache_guid, params.context,
            std::move(params.navigation_history));
    results.push_back(entry.get());
    if (is_ready_) {
      entries_[guid] = std::move(entry);
    } else {
      remote_entries_pending_model_ready_.push_back(std::move(entry));
    }
  }

  if (is_ready_) {
    for (auto& observer : observers_) {
      observer.OnEntriesAddedRemotely(results);
    }
  }

  return results;
}

void FakeSendTabToSelfModel::RemoveEntryRemotely(const std::string& guid) {
  auto it = entries_.find(guid);
  if (it != entries_.end()) {
    for (auto& observer : observers_) {
      observer.OnEntriesRemovedRemotely({guid});
    }
    entries_.erase(it);
  }
}

}  // namespace send_tab_to_self
