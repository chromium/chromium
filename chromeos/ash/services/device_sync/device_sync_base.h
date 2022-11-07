// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_
#define CHROMEOS_ASH_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_

#include "chromeos/ash/services/device_sync/public/mojom/device_sync.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {

namespace device_sync {

// Base DeviceSync implementation.
class DeviceSyncBase : public mojom::DeviceSync {
 public:
  DeviceSyncBase(const DeviceSyncBase&) = delete;
  DeviceSyncBase& operator=(const DeviceSyncBase&) = delete;

  ~DeviceSyncBase() override;

  // device_sync::mojom::DeviceSync:
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
};

}  // namespace device_sync

}  // namespace ash

#endif  // CHROMEOS_ASH_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_
