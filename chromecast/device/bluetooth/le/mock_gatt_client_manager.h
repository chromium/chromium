// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_GATT_CLIENT_MANAGER_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_GATT_CLIENT_MANAGER_H_

#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/device/bluetooth/le/gatt_client_manager.h"
#include "chromecast/device/bluetooth/le/mock_remote_device.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace bluetooth {

class MockGattClientManager : public GattClientManager {
 public:
  MockGattClientManager();
  ~MockGattClientManager() override;

  void AddObserver(Observer* o) override { observers_.AddObserver(o); }
  void RemoveObserver(Observer* o) override { observers_.RemoveObserver(o); }

  MOCK_METHOD(scoped_refptr<RemoteDevice>,
              GetDevice,
              (const bluetooth_v2_shlib::Addr& addr));
  void GetDevice(
      const bluetooth_v2_shlib::Addr& addr,
      base::OnceCallback<void(scoped_refptr<RemoteDevice>)> cb) override {
    std::move(cb).Run(GetDevice(addr));
  }

  MOCK_METHOD(scoped_refptr<RemoteDevice>,
              GetDeviceSync,
              (const bluetooth_v2_shlib::Addr& addr),
              (override));

  MOCK_METHOD(std::vector<scoped_refptr<RemoteDevice>>,
              GetConnectedDevices,
              ());
  void GetConnectedDevices(GetConnectDevicesCallback cb) override {
    std::move(cb).Run(GetConnectedDevices());
  }

  MOCK_METHOD(void,
              Initialize,
              (scoped_refptr<base::SingleThreadTaskRunner> io_task_runner),
              (override));
  MOCK_METHOD(void, Finalize, (), (override));
  MOCK_METHOD(void,
              GetNumConnected,
              (base::OnceCallback<void(size_t)> cb),
              (const, override));
  MOCK_METHOD(void,
              NotifyConnect,
              (const bluetooth_v2_shlib::Addr& addr),
              (override));
  MOCK_METHOD(void,
              NotifyBonded,
              (const bluetooth_v2_shlib::Addr& addr),
              (override));
  MOCK_METHOD(scoped_refptr<base::SingleThreadTaskRunner>,
              task_runner,
              (),
              (override));
  MOCK_METHOD(bool,
              IsConnectedLeDevice,
              (const bluetooth_v2_shlib::Addr& addr),
              (override));
  MOCK_METHOD(bool, SetGattClientConnectable, (bool connectable), (override));
  MOCK_METHOD(void, DisconnectAll, (StatusCallback cb), (override));

  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_GATT_CLIENT_MANAGER_H_
