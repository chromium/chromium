// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_BLE_ADVERTISER_H_
#define CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_BLE_ADVERTISER_H_

#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "chromeos/ash/services/secure_channel/ble_advertiser.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/ash/services/secure_channel/shared_resource_scheduler.h"

namespace ash::secure_channel {

// Test BleAdvertisementScheduler implementation, which internally uses a
// SharedResourceScheduler to store the provided requests.
class FakeBleAdvertiser : public BleAdvertiser {
 public:
  explicit FakeBleAdvertiser(Delegate* delegate);

  FakeBleAdvertiser(const FakeBleAdvertiser&) = delete;
  FakeBleAdvertiser& operator=(const FakeBleAdvertiser&) = delete;

  ~FakeBleAdvertiser() override;

  const std::list<DeviceIdPair>& GetRequestsForPriority(
      ConnectionPriority connection_priority);

  std::optional<ConnectionPriority> GetPriorityForRequest(
      const DeviceIdPair& request) const;

  std::vector<DeviceIdPair> GetAllRequestsForRemoteDevice(
      const std::string& remote_device_id);

  void NotifyAdvertisingSlotEnded(
      const DeviceIdPair& device_id_pair,
      bool replaced_by_higher_priority_advertisement);

  void NotifyFailureToGenerateAdvertisement(const DeviceIdPair& device_id_pair);

 private:
  // BleAdvertiser:
  void AddAdvertisementRequest(const DeviceIdPair& request,
                               ConnectionPriority connection_priority) override;
  void UpdateAdvertisementRequestPriority(
      const DeviceIdPair& request,
      ConnectionPriority connection_priority) override;
  void RemoveAdvertisementRequest(const DeviceIdPair& request) override;

  base::flat_map<ConnectionPriority, std::list<DeviceIdPair>>&
  priority_to_queued_requests_map() const {
    return scheduler_->priority_to_queued_requests_map_;
  }

  const base::flat_map<DeviceIdPair, ConnectionPriority>&
  request_to_priority_map() const {
    return scheduler_->request_to_priority_map_;
  }

  std::unique_ptr<SharedResourceScheduler> scheduler_;
};

// Test BleAdvertiser::Delegate implementation.
class FakeBleAdvertiserDelegate : public BleAdvertiser::Delegate {
 public:
  FakeBleAdvertiserDelegate();

  FakeBleAdvertiserDelegate(const FakeBleAdvertiserDelegate&) = delete;
  FakeBleAdvertiserDelegate& operator=(const FakeBleAdvertiserDelegate&) =
      delete;

  ~FakeBleAdvertiserDelegate() override;

  using SlotEndedEvent = std::pair<DeviceIdPair, bool>;

  const std::vector<SlotEndedEvent>& slot_ended_events() const {
    return slot_ended_events_;
  }

  const std::vector<DeviceIdPair>& advertisement_generation_failures() const {
    return advertisement_generation_failures_;
  }

 private:
  // BleAdvertiser::Delegate:
  void OnAdvertisingSlotEnded(
      const DeviceIdPair& device_id_pair,
      bool replaced_by_higher_priority_advertisement) override;
  void OnFailureToGenerateAdvertisement(
      const DeviceIdPair& device_id_pair) override;

  std::vector<SlotEndedEvent> slot_ended_events_;
  std::vector<DeviceIdPair> advertisement_generation_failures_;
};

}  // namespace ash::secure_channel

#endif  // CHROMEOS_ASH_SERVICES_SECURE_CHANNEL_FAKE_BLE_ADVERTISER_H_
