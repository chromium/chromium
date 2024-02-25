// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_H_
#define CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_H_

#include <optional>

#include "base/observer_list.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/multidevice_setup/public/mojom/multidevice_setup.mojom.h"

namespace ash {

namespace multidevice_setup {

// Provides the status of the current MultiDevice host, if it exists.
// Additionally, provides an observer interface for being notified when the host
// status changes.
class HostStatusProvider {
 public:
  class HostStatusWithDevice {
   public:
    HostStatusWithDevice(
        mojom::HostStatus host_status,
        const std::optional<multidevice::RemoteDeviceRef>& host_device);
    HostStatusWithDevice(const HostStatusWithDevice& other);
    ~HostStatusWithDevice();

    bool operator==(const HostStatusWithDevice& other) const;
    bool operator!=(const HostStatusWithDevice& other) const;

    mojom::HostStatus host_status() const { return host_status_; }

    // If host_status() is kNoEligibleHosts or
    // kEligibleHostExistsButNoHostSet, host_device() is null.
    const std::optional<multidevice::RemoteDeviceRef>& host_device() const {
      return host_device_;
    }

   private:
    mojom::HostStatus host_status_;
    std::optional<multidevice::RemoteDeviceRef> host_device_;
  };

  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnHostStatusChange(
        const HostStatusWithDevice& host_status_with_device) = 0;
  };

  HostStatusProvider(const HostStatusProvider&) = delete;
  HostStatusProvider& operator=(const HostStatusProvider&) = delete;

  virtual ~HostStatusProvider();

  virtual HostStatusWithDevice GetHostWithStatus() const = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  HostStatusProvider();

  void NotifyHostStatusChange(
      mojom::HostStatus host_status,
      const std::optional<multidevice::RemoteDeviceRef>& host_device);

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;
};

}  // namespace multidevice_setup

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_MULTIDEVICE_SETUP_HOST_STATUS_PROVIDER_H_
