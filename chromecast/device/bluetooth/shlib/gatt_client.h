// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_SHLIB_GATT_CLIENT_H_
#define CHROMECAST_DEVICE_BLUETOOTH_SHLIB_GATT_CLIENT_H_

#include <vector>

#include "chromecast/public/bluetooth/gatt.h"

namespace chromecast {
namespace bluetooth_v2_shlib {

class GattClient {
 public:
  virtual ~GattClient() = default;
  virtual bool IsSupported() = 0;
  virtual void SetDelegate(Gatt::Client::Delegate* delegate) = 0;
  virtual bool Connect(const Addr& addr, Gatt::Client::Transport transport) = 0;
  virtual bool Disconnect(const Addr& addr) = 0;
  virtual bool CreateBond(const Addr& addr) = 0;
  virtual bool RemoveBond(const Addr& addr) = 0;
  virtual bool ReadCharacteristic(const Addr& addr,
                                  const Gatt::Characteristic& characteristic,
                                  Gatt::Client::AuthReq auth_req) = 0;
  virtual bool WriteCharacteristic(const Addr& addr,
                                   const Gatt::Characteristic& characteristic,
                                   Gatt::Client::AuthReq auth_req,
                                   Gatt::WriteType write_type,
                                   const std::vector<uint8_t>& value) = 0;
  virtual bool ReadDescriptor(const Addr& addr,
                              const Gatt::Descriptor& descriptor,
                              Gatt::Client::AuthReq auth_req) = 0;
  virtual bool WriteDescriptor(const Addr& addr,
                               const Gatt::Descriptor& descriptor,
                               Gatt::Client::AuthReq auth_req,
                               const std::vector<uint8_t>& value) = 0;
  virtual bool SetCharacteristicNotification(
      const Addr& addr,
      const Gatt::Characteristic& characteristic,
      bool enable) = 0;
  virtual bool ReadRemoteRssi(const Addr& addr) = 0;
  virtual bool RequestMtu(const Addr& addr, int mtu) = 0;
  virtual bool ConnectionParameterUpdate(const Addr& addr,
                                         int min_interval,
                                         int max_interval,
                                         int latency,
                                         int timeout) = 0;
  virtual bool GetServices(const Addr& addr) = 0;
  virtual bool ClearPendingConnect(const Addr& addr) = 0;
  virtual bool ClearPendingDisconnect(const Addr& addr) = 0;
};

}  // namespace bluetooth_v2_shlib
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_SHLIB_GATT_CLIENT_H_
