// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_
#define CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/services/device_sync/public/mojom/device_sync.mojom.h"
#include "components/signin/core/browser/account_info.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace gcm {
class GCMAppHandler;
class GCMDriver;
}  // namespace gcm

namespace chromeos {

namespace device_sync {

// Base DeviceSync implementation.
class DeviceSyncBase : public mojom::DeviceSync {
 public:
  ~DeviceSyncBase() override;

  // mojom::DeviceSync:
  void AddObserver(mojom::DeviceSyncObserverPtr observer,
                   AddObserverCallback callback) override;

  // Binds a request to this implementation. Should be called each time that the
  // service receives a request.
  void BindRequest(mojom::DeviceSyncRequest request);

 protected:
  explicit DeviceSyncBase(gcm::GCMDriver* gcm_driver);

  // Derived types should override this function to remove references to any
  // dependencies.
  virtual void Shutdown() {}

  void NotifyOnEnrollmentFinished();
  void NotifyOnNewDevicesSynced();

 private:
  void OnDisconnection();

  mojo::InterfacePtrSet<mojom::DeviceSyncObserver> observers_;
  mojo::BindingSet<mojom::DeviceSync> bindings_;

  std::unique_ptr<gcm::GCMAppHandler> gcm_app_handler_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncBase);
};

}  // namespace device_sync

}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_DEVICE_SYNC_DEVICE_SYNC_BASE_H_
