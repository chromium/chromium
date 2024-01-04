// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_ACTIVE_DEVICES_PROVIDER_IMPL_H_
#define COMPONENTS_BROWSER_SYNC_ACTIVE_DEVICES_PROVIDER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "components/sync/service/active_devices_provider.h"
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
  syncer::ActiveDevicesInvalidationInfo CalculateInvalidationInfo(
      const std::string& local_cache_guid) const override;

  void SetActiveDevicesChangedCallback(
      ActiveDevicesChangedCallback callback) override;

  // syncer::DeviceInfoTracker::Observer implementation.
  void OnDeviceInfoChange() override;

 private:
  std::vector<const syncer::DeviceInfo*> GetActiveDevicesSortedByUpdateTime()
      const;

  const raw_ptr<syncer::DeviceInfoTracker> device_info_tracker_;
  const raw_ptr<const base::Clock> clock_;
  ActiveDevicesChangedCallback callback_;

  base::ScopedObservation<syncer::DeviceInfoTracker,
                          syncer::DeviceInfoTracker::Observer>
      device_info_tracker_observation_{this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_ACTIVE_DEVICES_PROVIDER_IMPL_H_
