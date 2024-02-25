// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_ble_advertiser.h"

#include "base/check.h"

namespace ash::secure_channel {

FakeBleAdvertiser::FakeBleAdvertiser(Delegate* delegate)
    : BleAdvertiser(delegate),
      scheduler_(std::make_unique<SharedResourceScheduler>()) {}

FakeBleAdvertiser::~FakeBleAdvertiser() = default;

const std::list<DeviceIdPair>& FakeBleAdvertiser::GetRequestsForPriority(
    ConnectionPriority connection_priority) {
  return priority_to_queued_requests_map()[connection_priority];
}

std::optional<ConnectionPriority> FakeBleAdvertiser::GetPriorityForRequest(
    const DeviceIdPair& request) const {
  for (auto it = request_to_priority_map().begin();
       it != request_to_priority_map().end(); ++it) {
    if (it->first == request)
      return it->second;
  }

  return std::nullopt;
}

std::vector<DeviceIdPair> FakeBleAdvertiser::GetAllRequestsForRemoteDevice(
    const std::string& remote_device_id) {
  std::vector<DeviceIdPair> all_requests_for_remote_device;
  for (const auto& map_entry : request_to_priority_map()) {
    if (map_entry.first.remote_device_id() == remote_device_id)
      all_requests_for_remote_device.push_back(map_entry.first);
  }
  return all_requests_for_remote_device;
}

void FakeBleAdvertiser::NotifyAdvertisingSlotEnded(
    const DeviceIdPair& device_id_pair,
    bool replaced_by_higher_priority_advertisement) {
  // |device_id_pair| must be scheduled.
  DCHECK(GetPriorityForRequest(device_id_pair));

  BleAdvertiser::NotifyAdvertisingSlotEnded(
      device_id_pair, replaced_by_higher_priority_advertisement);
}

void FakeBleAdvertiser::NotifyFailureToGenerateAdvertisement(
    const DeviceIdPair& device_id_pair) {
  // |device_id_pair| must be scheduled.
  DCHECK(GetPriorityForRequest(device_id_pair));

  BleAdvertiser::NotifyFailureToGenerateAdvertisement(device_id_pair);
}

void FakeBleAdvertiser::AddAdvertisementRequest(
    const DeviceIdPair& request,
    ConnectionPriority connection_priority) {
  scheduler_->ScheduleRequest(request, connection_priority);
}

void FakeBleAdvertiser::UpdateAdvertisementRequestPriority(
    const DeviceIdPair& request,
    ConnectionPriority connection_priority) {
  scheduler_->UpdateRequestPriority(request, connection_priority);
}

void FakeBleAdvertiser::RemoveAdvertisementRequest(
    const DeviceIdPair& request) {
  scheduler_->RemoveScheduledRequest(request);
}

FakeBleAdvertiserDelegate::FakeBleAdvertiserDelegate() = default;

FakeBleAdvertiserDelegate::~FakeBleAdvertiserDelegate() = default;

void FakeBleAdvertiserDelegate::OnAdvertisingSlotEnded(
    const DeviceIdPair& device_id_pair,
    bool replaced_by_higher_priority_advertisement) {
  slot_ended_events_.emplace_back(device_id_pair,
                                  replaced_by_higher_priority_advertisement);
}

void FakeBleAdvertiserDelegate::OnFailureToGenerateAdvertisement(
    const DeviceIdPair& device_id_pair) {
  advertisement_generation_failures_.emplace_back(device_id_pair);
}

}  // namespace ash::secure_channel
