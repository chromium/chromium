// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DEVICE_IMPL_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DEVICE_IMPL_H_

#include <atomic>
#include <deque>
#include <map>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "chromecast/device/bluetooth/le/remote_characteristic.h"
#include "chromecast/device/bluetooth/le/remote_descriptor.h"
#include "chromecast/device/bluetooth/le/remote_device.h"

namespace chromecast {
namespace bluetooth {

class GattClientManagerImpl;
class RemoteCharacteristicImpl;
class RemoteDescriptorImpl;

class RemoteDeviceImpl : public RemoteDevice {
 public:
  // If commands take longer than this amount of time, we will disconnect the
  // device.
  static constexpr base::TimeDelta kCommandTimeout =
      base::TimeDelta::FromSeconds(30);

  // RemoteDevice implementation
  void Connect(StatusCallback cb) override;
  void Disconnect(StatusCallback cb) override;
  void CreateBond(StatusCallback cb) override;
  void RemoveBond(StatusCallback cb) override;
  void ReadRemoteRssi(RssiCallback cb) override;
  void RequestMtu(int mtu, StatusCallback cb) override;
  void ConnectionParameterUpdate(int min_interval,
                                 int max_interval,
                                 int latency,
                                 int timeout,
                                 StatusCallback cb) override;
  bool IsConnected() override;
  bool IsBonded() override;
  int GetMtu() override;
  void GetServices(
      base::OnceCallback<void(std::vector<scoped_refptr<RemoteService>>)> cb)
      override;
  std::vector<scoped_refptr<RemoteService>> GetServicesSync() override;
  void GetServiceByUuid(
      const bluetooth_v2_shlib::Uuid& uuid,
      base::OnceCallback<void(scoped_refptr<RemoteService>)> cb) override;
  scoped_refptr<RemoteService> GetServiceByUuidSync(
      const bluetooth_v2_shlib::Uuid& uuid) override;
  const bluetooth_v2_shlib::Addr& addr() const override;

  void ReadCharacteristic(
      scoped_refptr<RemoteCharacteristicImpl> characteristic,
      bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
      RemoteCharacteristic::ReadCallback cb);
  void WriteCharacteristic(
      scoped_refptr<RemoteCharacteristicImpl> characteristic,
      bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
      bluetooth_v2_shlib::Gatt::WriteType write_type,
      std::vector<uint8_t> value,
      RemoteCharacteristic::StatusCallback cb);
  void ReadDescriptor(scoped_refptr<RemoteDescriptorImpl> descriptor,
                      bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                      RemoteDescriptor::ReadCallback cb);
  void WriteDescriptor(scoped_refptr<RemoteDescriptorImpl> descriptor,
                       bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                       std::vector<uint8_t> value,
                       RemoteDescriptor::StatusCallback cb);

 private:
  friend class GattClientManagerImpl;

  RemoteDeviceImpl(const bluetooth_v2_shlib::Addr& addr,
                   base::WeakPtr<GattClientManagerImpl> gatt_client_manager,
                   scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  ~RemoteDeviceImpl() override;

  // Friend methods for GattClientManagerImpl
  void SetConnected(bool connected);
  void SetBonded(bool bonded);
  void SetServicesDiscovered(bool discovered);
  bool GetServicesDiscovered();
  void SetMtu(int mtu);

  scoped_refptr<RemoteCharacteristic> CharacteristicFromHandle(uint16_t handle);

  void OnCharacteristicRead(bool status,
                            uint16_t handle,
                            const std::vector<uint8_t>& value);
  void OnCharacteristicWrite(bool status, uint16_t handle);
  void OnDescriptorRead(bool status,
                        uint16_t handle,
                        const std::vector<uint8_t>& value);
  void OnDescriptorWrite(bool status, uint16_t handle);
  void OnGetServices(
      const std::vector<bluetooth_v2_shlib::Gatt::Service>& services);
  void OnServicesRemoved(uint16_t start_handle, uint16_t end_handle);
  void OnServicesAdded(
      const std::vector<bluetooth_v2_shlib::Gatt::Service>& services);
  void OnReadRemoteRssiComplete(bool status, int rssi);
  // end Friend methods for GattClientManagerImpl

  void ConnectComplete(bool success);

  // Add an operation to the queue. Certain operations can only be executed
  // serially.
  void EnqueueOperation(const std::string& name, base::OnceClosure op);

  // Notify that the currently queued operation has completed.
  void NotifyQueueOperationComplete();

  // Run the next queued operation.
  void RunNextOperation();

  void RequestMtuImpl(int mtu);
  void ReadCharacteristicImpl(
      scoped_refptr<RemoteCharacteristicImpl> descriptor,
      bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req);
  void WriteCharacteristicImpl(
      scoped_refptr<RemoteCharacteristicImpl> descriptor,
      bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
      bluetooth_v2_shlib::Gatt::WriteType write_type,
      std::vector<uint8_t> value);
  void ReadDescriptorImpl(scoped_refptr<RemoteDescriptorImpl> descriptor,
                          bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req);
  void WriteDescriptorImpl(scoped_refptr<RemoteDescriptorImpl> descriptor,
                           bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
                           std::vector<uint8_t> value);
  void ClearServices();

  void OnCommandTimeout(const std::string& command_name);

  const base::WeakPtr<GattClientManagerImpl> gatt_client_manager_;
  const bluetooth_v2_shlib::Addr addr_;

  // All bluetooth_v2_shlib calls are run on this task_runner. Below members
  // should only be accessed on this task_runner.
  const scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  bool services_discovered_ = false;

  bool connect_pending_ = false;
  StatusCallback connect_cb_;

  bool disconnect_pending_ = false;
  StatusCallback disconnect_cb_;

  bool create_bond_pending_ = false;
  StatusCallback create_bond_cb_;

  bool remove_bond_pending_ = false;
  StatusCallback remove_bond_cb_;

  bool rssi_pending_ = false;
  RssiCallback rssi_cb_;

  std::atomic<bool> connected_{false};
  std::atomic<bool> bonded_{false};
  std::atomic<int> mtu_{kDefaultMtu};
  std::map<bluetooth_v2_shlib::Uuid, scoped_refptr<RemoteService>>
      uuid_to_service_;
  std::map<uint16_t, scoped_refptr<RemoteCharacteristicImpl>>
      handle_to_characteristic_;

  // Timer for commands on |command_queue_|. If any command times out, we will
  // force disconnect of this device.
  base::OneShotTimer command_timeout_timer_;

  // Queue of operation name and the operation itself.
  std::deque<std::pair<std::string, base::OnceClosure>> command_queue_;
  std::queue<StatusCallback> mtu_callbacks_;
  std::map<uint16_t, std::queue<RemoteCharacteristic::ReadCallback>>
      handle_to_characteristic_read_cbs_;
  std::map<uint16_t, std::queue<RemoteCharacteristic::StatusCallback>>
      handle_to_characteristic_write_cbs_;
  std::map<uint16_t, std::queue<RemoteDescriptor::ReadCallback>>
      handle_to_descriptor_read_cbs_;
  std::map<uint16_t, std::queue<RemoteDescriptor::StatusCallback>>
      handle_to_descriptor_write_cbs_;

  DISALLOW_COPY_AND_ASSIGN(RemoteDeviceImpl);
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_REMOTE_DEVICE_IMPL_H_
