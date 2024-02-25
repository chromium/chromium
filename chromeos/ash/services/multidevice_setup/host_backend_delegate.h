// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_BACKEND_DELEGATE_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_BACKEND_DELEGATE_H_

#include <optional>

#include "base/observer_list.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"

namespace ash {

namespace multidevice_setup {

// Delegate for setting and receiving the MultiDevice host from the back-end.
// This class is considered the source of truth for the most recent snapshot of
// what the server knows about.
class HostBackendDelegate {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    // Invoked when the host has changed. The new host can be retrieved via
    // GetMultiDeviceHostFromBackend().
    //
    // Note that this function is invoked when the host changes from one device
    // to another, from a device to no device at all, or from no device at all
    // to a device. The function is not invoked when an individual property of
    // the host device changes (i.e., this function is only invoked when the
    // previous host's device ID is different fro the new host's device ID).
    virtual void OnHostChangedOnBackend() {}

    // Invoked when an attempt to set the MultiDevice host failed. The device
    // whose attempt failed can be retrieved via GetPendingHostRequest().
    virtual void OnBackendRequestFailed() {}

    // Invoked when the pending host request has changed. Note that this
    // callback is also fired when a HasPendingHostRequest() changes from true
    // to false.
    virtual void OnPendingHostRequestChange() {}
  };

  HostBackendDelegate(const HostBackendDelegate&) = delete;
  HostBackendDelegate& operator=(const HostBackendDelegate&) = delete;

  virtual ~HostBackendDelegate();

  // Attempts to set |host_device| as the host on the back-end. If |host_device|
  // is null, this function attempts to remove the active host device.
  //
  // If the request is successful, the OnHostChangedOnBackend() observer
  // function is invoked.
  //
  // If a the request fails (e.g., the device is offline or the server is down),
  // the OnBackendRequestFailed() observer function is invoked, but this
  // object continues to attempt the request until the request succeeds or until
  // AttemptToSetMultiDeviceHostOnBackend() is called with a new device.
  //
  // If there is already a pending request and this function is called with the
  // same request, a retry will be attempted immediately.
  virtual void AttemptToSetMultiDeviceHostOnBackend(
      const std::optional<multidevice::RemoteDeviceRef>& host_device) = 0;

  // Returns whether there is a pending request to set the host on the back-end
  // which has not yet completed.
  virtual bool HasPendingHostRequest() = 0;

  // Returns the device which is pending to be set as the host device. If null
  // is returned, this means that the current host is pending removal.
  //
  // This function invokes a crash if called when HasPendingHostRequest()
  // returns false.
  virtual std::optional<multidevice::RemoteDeviceRef> GetPendingHostRequest()
      const = 0;

  // Provides the host from the most recent device sync. If the return value is
  // null, there is no host set on the back-end.
  virtual std::optional<multidevice::RemoteDeviceRef>
  GetMultiDeviceHostFromBackend() const = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  HostBackendDelegate();

  void NotifyHostChangedOnBackend();
  void NotifyBackendRequestFailed();
  void NotifyPendingHostRequestChange();

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_BACKEND_DELEGATE_H_
