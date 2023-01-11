// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_GATT_CLIENT_MANAGER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_GATT_CLIENT_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_device.h"
#include "chromecast/device/bluetooth/le/remote_service.h"
#include "chromecast/public/bluetooth/bluetooth_types.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace chromecast {
namespace bluetooth_v2_shlib {
class GattClient;
}  // namespace bluetooth_v2_shlib

namespace bluetooth {

class BluetoothManagerPlatform;
class LeScanManager;

class GattClientManager {
 public:
  class Observer {
   public:
    // Called when the connection state changes for |device|.
    virtual void OnConnectChanged(scoped_refptr<RemoteDevice> device,
                                  bool connected) {}

    // Called when the bond state changes for |device|.
    virtual void OnBondChanged(scoped_refptr<RemoteDevice> device,
                               bool bonded) {}

    // Called when the connection MTU changes for |device|.
    virtual void OnMtuChanged(scoped_refptr<RemoteDevice> device, int mtu) {}

    // Called when the device |device|'s service list changed. |services| is the
    // new list of services, and any old ones should be considered invalidated.
    virtual void OnServicesUpdated(
        scoped_refptr<RemoteDevice> device,
        std::vector<scoped_refptr<RemoteService>> services) {}

    // Called when |device| has a notification on |characteristic| with |value|.
    virtual void OnCharacteristicNotification(
        scoped_refptr<RemoteDevice> device,
        scoped_refptr<RemoteCharacteristic> characteristic,
        std::vector<uint8_t> value) {}

    // Called when a connection is requested.
    virtual void OnConnectInitated(const bluetooth_v2_shlib::Addr& addr) {}

    virtual ~Observer() = default;
  };

  static std::unique_ptr<GattClientManager> Create(
      bluetooth_v2_shlib::GattClient* gatt_client,
      BluetoothManagerPlatform* bluetooth_manager,
      LeScanManager* le_scan_manager);

  GattClientManager(const GattClientManager&) = delete;
  GattClientManager& operator=(const GattClientManager&) = delete;

  virtual ~GattClientManager() = default;

  virtual void Initialize(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) = 0;
  virtual void Finalize() = 0;

  virtual void AddObserver(Observer* o) = 0;
  virtual void RemoveObserver(Observer* o) = 0;

  // Get a RemoteDevice object corresponding to |addr| for performing GATT
  // operations. |cb| will be run on the callers thread. Callbacks passed into
  // methods on RemoteDevice and its subobjects (RemoteService,
  // RemoteCharacteristic, RemoteDescriptor) will also be run on the thread
  // which called the specific method.
  virtual void GetDevice(
      const bluetooth_v2_shlib::Addr& addr,
      base::OnceCallback<void(scoped_refptr<RemoteDevice>)> cb) = 0;

  // TODO(bcf): Deprecated. Replace usage with async version.
  virtual scoped_refptr<RemoteDevice> GetDeviceSync(
      const bluetooth_v2_shlib::Addr& addr) = 0;

  // Returns the currently connected devices.
  using GetConnectDevicesCallback =
      base::OnceCallback<void(std::vector<scoped_refptr<RemoteDevice>>)>;
  virtual void GetConnectedDevices(GetConnectDevicesCallback cb) = 0;

  // Returns the number of devices which are currently connected.
  // TODO(bcf): Deprecated in favor of |GetConnectedDevices|.
  virtual void GetNumConnected(base::OnceCallback<void(size_t)> cb) const = 0;

  // Called when we initiate connection to a remote device.
  virtual void NotifyConnect(const bluetooth_v2_shlib::Addr& addr) = 0;

  // Used to notify |this| of currently bonded devices on initialization.
  // Note that these devices might not be connected.
  virtual void NotifyBonded(const bluetooth_v2_shlib::Addr& addr) = 0;

  // Returns true if |addr| corresponds to a connected BLE device.
  virtual bool IsConnectedLeDevice(const bluetooth_v2_shlib::Addr& addr) = 0;

  // Enable or disable GATT client connectability. Returns |true| if successful
  // otherwise |false|.
  virtual bool SetGattClientConnectable(bool connectable) = 0;

  // Disconnect all connected devices. Callback will return |true| if all
  // devices are disconnected, otherwise false.
  // When disabling GATT client, caller should call
  // SetGattClientConnectable(false) before calling DisconnectAll so that
  // upcoming GATT client connections can also be blocked.
  using StatusCallback = base::OnceCallback<void(bool)>;
  virtual void DisconnectAll(StatusCallback cb) = 0;

  // TODO(bcf): Deprecated. Should be removed now that this class may be used
  // from any thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner> task_runner() = 0;

 protected:
  GattClientManager() = default;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_GATT_CLIENT_MANAGER_H_
