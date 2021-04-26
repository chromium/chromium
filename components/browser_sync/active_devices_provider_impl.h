// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_ACTIVE_DEVICES_PROVIDER_IMPL_H_
#define COMPONENTS_BROWSER_SYNC_ACTIVE_DEVICES_PROVIDER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/sequence_checker.h"
#include "base/time/default_clock.h"
#include "components/sync/driver/active_devices_provider.h"
#include "components/sync_device_info/device_info_tracker.h"

namespace browser_sync {

class ActiveDevicesProviderImpl : public syncer::ActiveDevicesProvider,
                                  public syncer::DeviceInfoTracker::Observer {
 public:
  ActiveDevicesProviderImpl(syncer::DeviceInfoTracker* device_info_tracker,
                            base::Clock* clock);
  ActiveDevicesProviderImpl(const ActiveDevicesProviderImpl&) = delete;
  ActiveDevicesProviderImpl& operator=(const ActiveDevicesProviderImpl&) =
      delete;

  ~ActiveDevicesProviderImpl() override;

  // syncer::ActiveDevicesProvider implementation.
  size_t CountActiveDevicesIfAvailable() override;

  std::vector<std::string> CollectFCMRegistrationTokensForInvalidations(
      const std::string& local_cache_guid) override;

  void SetActiveDevicesChangedCallback(
      ActiveDevicesChangedCallback callback) override;

  // syncer::DeviceInfoTracker::Observer implementation.
  void OnDeviceInfoChange() override;

 private:
  std::vector<std::unique_ptr<syncer::DeviceInfo>> GetActiveDevices() const;

  syncer::DeviceInfoTracker* const device_info_tracker_;
  const base::Clock* const clock_;
  ActiveDevicesChangedCallback callback_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_ACTIVE_DEVICES_PROVIDER_IMPL_H_
