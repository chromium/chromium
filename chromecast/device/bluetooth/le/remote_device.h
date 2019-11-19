// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DEVICE_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DEVICE_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chromecast/public/bluetooth/gatt.h"

namespace chromecast {
namespace bluetooth {

class RemoteService;

// A proxy to for a remote GATT server device.
class RemoteDevice : public base::RefCountedThreadSafe<RemoteDevice> {
 public:
  enum : int {
    kDefaultMtu = 20,
  };

  using StatusCallback = base::OnceCallback<void(bool)>;

  // Initiate a connection to this device. Callback will return |true| if
  // connected successfully, otherwise false. Only one pending call is allowed
  // at a time.
  virtual void Connect(StatusCallback cb) = 0;

  // Disconnect from this device. Callback will return |true| if disconnected
  // successfully, otherwise false. Only one pending call is allowed at a time.
  virtual void Disconnect(StatusCallback cb) = 0;

  // Create bond to this device. Callback will return |true| if
  // bonded successfully, otherwise false. Device must be connected.
  virtual void CreateBond(StatusCallback cb) = 0;

  // Remove bond to this device. Callback will return |true| if
  // bond is removed, otherwise false.
  virtual void RemoveBond(StatusCallback cb) = 0;

  // Read this device's RSSI. The result will be sent in |callback|. Only one
  // pending call is allowed at a time.
  using RssiCallback = base::OnceCallback<void(bool success, int rssi)>;
  virtual void ReadRemoteRssi(RssiCallback cb) = 0;

  // Request an MTU update to |mtu|. Callback will return |true| if MTU is
  // updated successfully, otherwise false. Only one pending call is allowed at
  // a time.
  virtual void RequestMtu(int mtu, StatusCallback cb) = 0;

  // Request an update to connection parameters.
  virtual void ConnectionParameterUpdate(int min_interval,
                                         int max_interval,
                                         int latency,
                                         int timeout,
                                         StatusCallback cb) = 0;

  // Returns true if this device is connected.
  virtual bool IsConnected() = 0;

  // Returns true if this device is bonded.
  virtual bool IsBonded() = 0;

  // Returns the current MTU of the connection with this device.
  virtual int GetMtu() = 0;

  // Returns a list of all discovered services on this device. After
  // GattClientManager::Observer::OnServicesUpdated is called, these may point
  // to old services, so services need to be reobtained.
  virtual void GetServices(
      base::OnceCallback<void(std::vector<scoped_refptr<RemoteService>>)>
          cb) = 0;

  // TODO(bcf): Deprecated. Replace usage with async version.
  virtual std::vector<scoped_refptr<RemoteService>> GetServicesSync() = 0;

  // Returns the service corresponding to |uuid|, or nullptr if none exist.
  virtual void GetServiceByUuid(
      const bluetooth_v2_shlib::Uuid& uuid,
      base::OnceCallback<void(scoped_refptr<RemoteService>)> cb) = 0;

  // TODO(bcf): Deprecated. Replace usage with async version.
  virtual scoped_refptr<RemoteService> GetServiceByUuidSync(
      const bluetooth_v2_shlib::Uuid& uuid) = 0;

  virtual const bluetooth_v2_shlib::Addr& addr() const = 0;

 protected:
  friend base::RefCountedThreadSafe<RemoteDevice>;

  RemoteDevice() = default;
  virtual ~RemoteDevice() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteDevice);
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DEVICE_H_
