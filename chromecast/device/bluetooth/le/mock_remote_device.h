// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_DEVICE_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_DEVICE_H_

#include <vector>

#include "chromecast/device/bluetooth/le/remote_device.h"
#include "chromecast/device/bluetooth/le/remote_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {
namespace bluetooth {

class MockRemoteDevice : public RemoteDevice {
 public:
  explicit MockRemoteDevice(const bluetooth_v2_shlib::Addr& addr);

  MOCK_METHOD0(Connect, ConnectStatus());
  void Connect(ConnectCallback cb,
               bluetooth_v2_shlib::Gatt::Client::Transport /* transport */) override {
    std::move(cb).Run(Connect());
  }

  MOCK_METHOD0(Disconnect, bool());
  void Disconnect(StatusCallback cb) override {
    std::move(cb).Run(Disconnect());
  }

  MOCK_METHOD0(CreateBond, bool());
  void CreateBond(StatusCallback cb) override {
    std::move(cb).Run(CreateBond());
  }

  MOCK_METHOD0(RemoveBond, bool());
  void RemoveBond(StatusCallback cb) override {
    std::move(cb).Run(RemoveBond());
  }

  MOCK_METHOD1(ReadRemoteRssi, void(RssiCallback cb));

  MOCK_METHOD1(RequestMtu, bool(int mtu));
  void RequestMtu(int mtu, StatusCallback cb) override {
    std::move(cb).Run(RequestMtu(mtu));
  }

  MOCK_METHOD4(
      ConnectionParameterUpdate,
      bool(int min_interval, int max_interval, int latency, int timeout));
  void ConnectionParameterUpdate(int min_interval,
                                 int max_interval,
                                 int latency,
                                 int timeout,
                                 StatusCallback cb) override {
    std::move(cb).Run(ConnectionParameterUpdate(min_interval, max_interval,
                                                latency, timeout));
  }

  MOCK_METHOD0(IsConnected, bool());

  MOCK_METHOD0(IsBonded, bool());

  MOCK_METHOD0(GetMtu, int());

  MOCK_METHOD0(GetServices, std::vector<scoped_refptr<RemoteService>>());
  void GetServices(
      base::OnceCallback<void(std::vector<scoped_refptr<RemoteService>>)> cb)
      override {
    std::move(cb).Run(GetServices());
  }

  MOCK_METHOD0(GetServicesSync, std::vector<scoped_refptr<RemoteService>>());

  MOCK_METHOD1(
      GetServiceByUuid,
      scoped_refptr<RemoteService>(const bluetooth_v2_shlib::Uuid& uuid));
  void GetServiceByUuid(
      const bluetooth_v2_shlib::Uuid& uuid,
      base::OnceCallback<void(scoped_refptr<RemoteService>)> cb) override {
    std::move(cb).Run(GetServiceByUuid(uuid));
  }

  MOCK_METHOD1(
      GetServiceByUuidSync,
      scoped_refptr<RemoteService>(const bluetooth_v2_shlib::Uuid& uuid));

  const bluetooth_v2_shlib::Addr& addr() const override { return addr_; }

  const bluetooth_v2_shlib::Addr addr_;

 private:
  friend testing::StrictMock<MockRemoteDevice>;

  ~MockRemoteDevice() override;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_MOCK_REMOTE_DEVICE_H_
