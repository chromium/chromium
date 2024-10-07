// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_TRACKER_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_TRACKER_H_

#include <map>
#include <string>
#include <vector>

#include "components/sync_device_info/device_info.h"

namespace sync_pb {
enum SyncEnums_DeviceType : int;
}  // namespace sync_pb

namespace syncer {

// Interface for tracking synced DeviceInfo. Note that this includes sync-ing
// clients that are not chromium-based.
class DeviceInfoTracker {
 public:
  virtual ~DeviceInfoTracker() = default;

  // Observer class for listening to device info changes.
  class Observer {
   public:
    // Called on any change in the device info list. If sync is enabled, it is
    // guaranteed that the method will be called at least once after browser
    // startup. There are several possible cases:
    // 1. The list has been changed during remote update (initial merge or
    // incremental).
    // 2. The list has been cleaned up when sync is stopped.
    // 3. The local device info has been changed and committed to the server.
    // 4. The list has been just loaded after browser startup from the
    // persistent storage. If the list is empty (e.g. due to mismatching cache
    // GUID or this is the first browser startup), it will be updated later
    // during the initial merge.
    virtual void OnDeviceInfoChange() = 0;

    // Called before the device info list is destroyed. Enables clients holding
    // raw pointers to DeviceInfo/DeviceInfoTracker(s) to null them at the
    // proper time, and not hold garbage pointers.
    //
    // TODO(crbug.com/40250371): Remove OnDeviceInfoShutdown() once proper
    // DependsOn() relationship exists between KeyedServices.
    virtual void OnDeviceInfoShutdown() {}

    virtual ~Observer() = default;
  };

  // Returns true when DeviceInfo datatype is enabled and syncing.
  virtual bool IsSyncing() const = 0;
  // Gets DeviceInfo the synced device with specified client ID.
  // Returns null if device with the given |client_id| hasn't been synced.
  // The returned pointer is meant to be short-lived (i.e. use only within the
  // ongoing task) and may be dangling otherwise.
  virtual const DeviceInfo* GetDeviceInfo(
      const std::string& client_id) const = 0;
  // Gets DeviceInfo for all synced devices (including the local one). The
  // returned pointers are meant to be short-lived (i.e. use only within the
  // ongoing task) and may be dangling otherwise.
  virtual std::vector<const DeviceInfo*> GetAllDeviceInfo() const = 0;
  // Same as above but returns only DeviceInfo for Chrome clients.
  virtual std::vector<const DeviceInfo*> GetAllChromeDeviceInfo() const = 0;
  // Registers an observer to be called on syncing any updated DeviceInfo.
  virtual void AddObserver(Observer* observer) = 0;
  // Unregisters an observer.
  virtual void RemoveObserver(Observer* observer) = 0;
  // Returns the count of active devices per form factor; identified by the
  // OsType and the FormFactor. Deduping logic may be used internally to prevent
  // double counting for devices that disable sync and reenable it, but callers
  // should nevertheless consider this an upper bound per type.
  virtual std::map<DeviceInfo::FormFactor, int> CountActiveDevicesByType()
      const = 0;
  // A function to to allow tests to ensure active devices. If called when the
  // local device info provider is not initialized, will force update after
  // initialization.
  virtual void ForcePulseForTest() = 0;
  // Returns if the provided |cache_guid| is the current device cache_guid for
  // the current device or was of the recently used.
  virtual bool IsRecentLocalCacheGuid(const std::string& cache_guid) const = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_TRACKER_H_
