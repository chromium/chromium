// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_ACTIVE_DEVICES_PROVIDER_H_
#define COMPONENTS_SYNC_DRIVER_ACTIVE_DEVICES_PROVIDER_H_

#include "base/callback.h"

namespace syncer {

// An interface helping to get the number of active devices. Devices are
// considered active if there are DeviceInfo entries that are (typically) less
// than one day old (with a little margin around half an hour).
class ActiveDevicesProvider {
 public:
  using ActiveDevicesChangedCallback = base::RepeatingClosure;

  virtual ~ActiveDevicesProvider() = default;

  // Returns number of active devices or 0 if number of active devices is not
  // known yet (e.g. data types are not configured).
  virtual size_t CountActiveDevicesIfAvailable() = 0;

  // The |callback| will be called on each change in device infos. It might be
  // called multiple times with the same number of active devices. The
  // |callback| must be cleared before this object is destroyed.
  virtual void SetActiveDevicesChangedCallback(
      ActiveDevicesChangedCallback callback) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_ACTIVE_DEVICES_PROVIDER_H_
