// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/stub_send_tab_to_self_sync_service.h"

#include <utility>

#include "base/time/time.h"
#include "components/send_tab_to_self/fake_send_tab_to_self_model.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "components/sync/base/data_type.h"
#include "components/sync_device_info/device_info.h"
#include "url/gurl.h"

namespace send_tab_to_self {

StubSendTabToSelfSyncService::StubSendTabToSelfSyncService()
    : fake_delegate_(syncer::SEND_TAB_TO_SELF),
      entry_point_display_reason_(EntryPointDisplayReason::kOfferFeature) {
  UpdateTargetDevicesForDisplayReason();
}

StubSendTabToSelfSyncService::~StubSendTabToSelfSyncService() = default;

std::optional<EntryPointDisplayReason>
StubSendTabToSelfSyncService::GetEntryPointDisplayReason(
    const GURL& url_to_share) {
  return entry_point_display_reason_;
}

void StubSendTabToSelfSyncService::SetEntryPointDisplayReason(
    std::optional<EntryPointDisplayReason> reason) {
  entry_point_display_reason_ = reason;
  UpdateTargetDevicesForDisplayReason();
}

SendTabToSelfModel* StubSendTabToSelfSyncService::GetSendTabToSelfModel() {
  return &model_;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
StubSendTabToSelfSyncService::GetControllerDelegate() {
  return fake_delegate_.GetWeakPtr();
}

FakeSendTabToSelfModel*
StubSendTabToSelfSyncService::GetFakeSendTabToSelfModel() {
  return &model_;
}

void StubSendTabToSelfSyncService::UpdateTargetDevicesForDisplayReason() {
  if (entry_point_display_reason_ == EntryPointDisplayReason::kOfferFeature) {
    if (model_.GetTargetDeviceInfoSortedList().empty()) {
      model_.SetTargetDeviceInfoSortedList({TargetDeviceInfo(
          "Device", "guid", syncer::DeviceInfo::FormFactor::kDesktop,
          syncer::DeviceInfo::OsType::kLinux, base::Time::Now())});
    }
  } else {
    model_.SetTargetDeviceInfoSortedList({});
  }
}

}  // namespace send_tab_to_self
