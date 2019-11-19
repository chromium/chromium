// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {

namespace device_sync {

// Base DeviceSync implementation.
class DeviceSyncBase : public mojom::DeviceSync {
 public:
  ~DeviceSyncBase() override;

  // mojom::DeviceSync:
  void AddObserver(mojo::PendingRemote<mojom::DeviceSyncObserver> observer,
                   AddObserverCallback callback) override;

  // Binds a receiver to this implementation. Should be called each time that
  // the service receives a receiver.
  void BindReceiver(mojo::PendingReceiver<mojom::DeviceSync> receiver);

  void CloseAllReceivers();

 protected:
  DeviceSyncBase();

  // Derived types should override this function to remove references to any
  // dependencies.
  virtual void Shutdown() {}

  void NotifyOnEnrollmentFinished();
  void NotifyOnNewDevicesSynced();

 private:
  void OnDisconnection();

  mojo::RemoteSet<mojom::DeviceSyncObserver> observers_;
  mojo::ReceiverSet<mojom::DeviceSync> receivers_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncBase);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_
