// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SERVICE_ACTIVE_DEVICES_PROVIDER_H_
#define COMPONENTS_SYNC_SERVICE_ACTIVE_DEVICES_PROVIDER_H_

#include <string>

#include "base/functional/callback.h"
#include "components/sync/engine/active_devices_invalidation_info.h"

namespace syncer {

// An interface helping to get the information about active devices. Devices are
// considered active if there are DeviceInfo entries that are (typically) less
// than one day old (with a little margin around half an hour).
class ActiveDevicesProvider {
 public:
  using ActiveDevicesChangedCallback = base::RepeatingClosure;

  virtual ~ActiveDevicesProvider() = default;

  // Prepare information for the following sync cycles about invalidations on
  // other devices.
  virtual ActiveDevicesInvalidationInfo CalculateInvalidationInfo(
      const std::string& local_cache_guid) const = 0;

  // The |callback| will be called on each change in device infos. It might be
  // called multiple times with the same number of active devices. The
  // |callback| must be cleared before this object is destroyed.
  virtual void SetActiveDevicesChangedCallback(
      ActiveDevicesChangedCallback callback) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_SERVICE_ACTIVE_DEVICES_PROVIDER_H_
