// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TransientDeviceIds keep track of transient IDs for removable devices, so
// persistent device IDs are not exposed to renderers. Once a removable device
// gets mapped to a transient ID, the mapping remains valid for the duration of
// TransientDeviceIds' lifetime.

#ifndef COMPONENTS_STORAGE_MONITOR_TRANSIENT_DEVICE_IDS_H_
#define COMPONENTS_STORAGE_MONITOR_TRANSIENT_DEVICE_IDS_H_

#include <map>
#include <string>

#include "base/threading/thread_checker.h"

namespace storage_monitor {

class TransientDeviceIds {
 public:
  TransientDeviceIds();

  TransientDeviceIds(const TransientDeviceIds&) = delete;
  TransientDeviceIds& operator=(const TransientDeviceIds&) = delete;

  ~TransientDeviceIds();

  // Returns the transient ID for a given |device_id|.
  // |device_id| must be for a fixed or removable storage device.
  // If |device_id| has never been seen before, a new, unique transient id will
  // be assigned.
  std::string GetTransientIdForDeviceId(const std::string& device_id);

  // Get the reverse mapping for a transient ID. Returns an empty string if the
  // |transient_id| cannot be found.
  std::string DeviceIdFromTransientId(const std::string& transient_id) const;

 private:
  typedef std::map<std::string, std::string> IdMap;

  IdMap device_id_map_;
  IdMap transient_id_map_;

  base::ThreadChecker thread_checker_;
};

}  // namespace storage_monitor

#endif  // COMPONENTS_STORAGE_MONITOR_TRANSIENT_DEVICE_IDS_H_
