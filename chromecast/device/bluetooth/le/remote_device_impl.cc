// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/remote_device_impl.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/device/bluetooth/le/gatt_client_manager_impl.h"
#include "chromecast/device/bluetooth/le/remote_characteristic_impl.h"
#include "chromecast/device/bluetooth/le/remote_descriptor_impl.h"
#include "chromecast/device/bluetooth/le/remote_service_impl.h"

namespace chromecast {
namespace bluetooth {

#define RUN_ON_IO_THREAD(method, ...) \
  io_task_runner_->PostTask(          \
      FROM_HERE,                      \
      base::BindOnce(&RemoteDeviceImpl::method, this, ##__VA_ARGS__));

#define MAKE_SURE_IO_THREAD(method, ...)            \
  DCHECK(io_task_runner_);                          \
  if (!io_task_runner_->BelongsToCurrentThread()) { \
    RUN_ON_IO_THREAD(method, ##__VA_ARGS__)         \
    return;                                         \
  }

#define EXEC_CB_AND_RET(cb, ret, ...)        \
  do {                                       \
    if (cb) {                                \
      std::move(cb).Run(ret, ##__VA_ARGS__); \
    }                                        \
    return;                                  \
  } while (0)

#define CHECK_CONNECTED(cb)                              \
  do {                                                   \
    if (!connected_) {                                   \
      LOG(ERROR) << __func__ << "failed: Not connected"; \
      EXEC_CB_AND_RET(cb, false);                        \
    }                                                    \
  } while (0)

#define LOG_EXEC_CB_AND_RET(cb, ret)      \
  do {                                    \
    if (!ret) {                           \
      LOG(ERROR) << __func__ << "failed"; \
    }                                     \
    EXEC_CB_AND_RET(cb, ret);             \
  } while (0)

// static
constexpr base::TimeDelta RemoteDeviceImpl::kCommandTimeout;

RemoteDeviceImpl::RemoteDeviceImpl(
    const bluetooth_v2_shlib::Addr& addr,
    base::WeakPtr<GattClientManagerImpl> gatt_client_manager,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : gatt_client_manager_(gatt_client_manager),
      addr_(addr),
      io_task_runner_(io_task_runner) {
  DCHECK(gatt_client_manager);
  DCHECK(io_task_runner_->BelongsToCurrentThread());
}

RemoteDeviceImpl::~RemoteDeviceImpl() = default;

void RemoteDeviceImpl::Connect(StatusCallback cb) {
  MAKE_SURE_IO_THREAD(Connect, BindToCurrentSequence(std::move(cb)));
  LOG(INFO) << "Connect(" << util::AddrLastByteString(addr_) << ")";

  if (!gatt_client_manager_) {
    LOG(ERROR) << __func__ << " failed: Destroyed";
    EXEC_CB_AND_RET(cb, false);
  }

  if (connect_pending_) {
    LOG(ERROR) << __func__ << " failed: Connection pending";
    EXEC_CB_AND_RET(cb, false);
  }

  gatt_client_manager_->NotifyConnect(addr_);
  connect_pending_ = true;
  connect_cb_ = std::move(cb);
  gatt_client_manager_->EnqueueConnectRequest(addr_, true);
}

void RemoteDeviceImpl::Disconnect(StatusCallback cb) {
  MAKE_SURE_IO_THREAD(Disconnect, BindToCurrentSequence(std::move(cb)));
  LOG(INFO) << "Disconnect(" << util::AddrLastByteString(addr_) << ")";

  if (!gatt_client_manager_) {
    LOG(ERROR) << __func__ << " failed: Destroyed";
    EXEC_CB_AND_RET(cb, false);
  }

  disconnect_pending_ = true;
  disconnect_cb_ = std::move(cb);
  gatt_client_manager_->EnqueueConnectRequest(addr_, false);
}

void RemoteDeviceImpl::CreateBond(StatusCallback cb) {
  MAKE_SURE_IO_THREAD(CreateBond, BindToCurrentSequence(std::move(cb)));
  LOG(INFO) << "CreateBond(" << util::AddrLastByteString(addr_) << ")";
  if (!gatt_client_manager_) {
    LOG(ERROR) << __func__ << " failed: Destroyed";
    EXEC_CB_AND_RET(cb, false);
  }

  if (!connected_) {
    LOG(ERROR) << "Not connected";
    EXEC_CB_AND_RET(cb, false);
  }

  if (create_bond_pending_ || remove_bond_pending_) {
    // TODO(tiansong): b/120489954 Implement queuing and timeout logic.
    LOG(ERROR) << __func__ << " failed: waiting for pending bond command";
    EXEC_CB_AND_RET(cb, false);
  }

  if (bonded_) {
    LOG(ERROR) << "Already bonded";
    EXEC_CB_AND_RET(cb, false);
  }

  if (!gatt_client_manager_->gatt_client()->CreateBond(addr_)) {
    LOG(ERROR) << __func__ << " failed";
    EXEC_CB_AND_RET(cb, false);
  }

  create_bond_pending_ = true;
  create_bond_cb_ = std::move(cb);
}

void RemoteDeviceImpl::RemoveBond(StatusCallback cb) {
  MAKE_SURE_IO_THREAD(RemoveBond, BindToCurrentSequence(std::move(cb)));
  LOG(INFO) << "RemoveBond(" << util::AddrLastByteString(addr_) << ")";
  if (!gatt_client_manager_) {
    LOG(ERROR) << __func__ << " failed: Destroyed";
    EXEC_CB_AND_RET(cb, false);
  }

  if (create_bond_pending_ || remove_bond_pending_) {
    // TODO(tiansong): b/120489954 Implement queuing and timeout logic.
    LOG(ERROR) << __func__ << " failed: waiting for pending bond command";
    EXEC_CB_AND_RET(cb, false);
  }

  if (!bonded_) {
    LOG(WARNING) << "Not bonded";
  }

  if (!gatt_client_manager_->gatt_client()->RemoveBond(addr_)) {
    LOG(ERROR) << __func__ << " failed";
    EXEC_CB_AND_RET(cb, false);
  }

  remove_bond_pending_ = true;
  remove_bond_cb_ = std::move(cb);
}

void RemoteDeviceImpl::ReadRemoteRssi(RssiCallback cb) {
  MAKE_SURE_IO_THREAD(ReadRemoteRssi, BindToCurrentSequence(std::move(cb)));
  if (!gatt_client_manager_) {
    LOG(ERROR) << __func__ << " failed: Destroyed";
    EXEC_CB_AND_RET(cb, false, 0);
  }

  if (rssi_pending_) {
    LOG(ERROR) << "Read remote RSSI already pending";
    EXEC_CB_AND_RET(cb, false, 0);
  }

  rssi_pending_ = true;
  rssi_cb_ = std::move(cb);
  gatt_client_manager_->EnqueueReadRemoteRssiRequest(addr_);
}

void RemoteDeviceImpl::RequestMtu(int mtu, StatusCallback cb) {
  MAKE_SURE_IO_THREAD(RequestMtu, mtu, BindToCurrentSequence(std::move(cb)));
  LOG(INFO) << "RequestMtu(" << util::AddrLastByteString(addr_) << ", " << mtu
            << ")";
  DCHECK(cb);
  if (!gatt_client_manager_) {
    LOG(ERROR) << __func__ << " failed: Destroyed";
    EXEC_CB_AND_RET(cb, false);
  }
  CHECK_CONNECTED(cb);
  mtu_callbacks_.push(std::move(cb));
  EnqueueOperation(
      __func__, base::BindOnce(&RemoteDeviceImpl::RequestMtuImpl, this, mtu));
}

void RemoteDeviceImpl::ConnectionParameterUpdate(int min_interval,
                                                 int max_interval,
                                                 int latency,
                                                 int timeout,
                                                 StatusCallback cb) {
  MAKE_SURE_IO_THREAD(ConnectionParameterUpdate, min_interval, max_interval,
                      latency, timeout, BindToCurrentSequence(std::move(cb)));
  LOG(INFO) << "ConnectionParameterUpdate(" << util::AddrLastByteString(addr_)
            << ", " << min_interval << ", " << max_interval << ", " << latency
            << ", " << timeout << ")";
  if (!gatt_client_manager_) {
    LOG(ERROR) << __func__ << " failed: Destroyed";
    EXEC_CB_AND_RET(cb, false);
  }
  CHECK_CONNECTED(cb);
  bool ret = gatt_client_manager_->gatt_client()->ConnectionParameterUpdate(
      addr_, min_interval, max_interval, latency, timeout);
  LOG_EXEC_CB_AND_RET(cb, ret);
}

bool RemoteDeviceImpl::IsConnected() {
  return connected_ && !disconnect_pending_;
}

bool RemoteDeviceImpl::IsBonded() {
  return bonded_;
}

int RemoteDeviceImpl::GetMtu() {
  return mtu_;
}

void RemoteDeviceImpl::GetServices(
    base::OnceCallback<void(std::vector<scoped_refptr<RemoteService>>)> cb) {
  MAKE_SURE_IO_THREAD(GetServices, BindToCurrentSequence(std::move(cb)));
  auto ret = GetServicesSync();
  EXEC_CB_AND_RET(cb, std::move(ret));
}

std::vector<scoped_refptr<RemoteService>> RemoteDeviceImpl::GetServicesSync() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  std::vector<scoped_refptr<RemoteService>> services;
  services.reserve(uuid_to_service_.size());
  for (const auto& pair : uuid_to_service_)
    services.push_back(pair.second);

  return services;
}

void RemoteDeviceImpl::GetServiceByUuid(
    const bluetooth_v2_shlib::Uuid& uuid,
    base::OnceCallback<void(scoped_refptr<RemoteService>)> cb) {
  MAKE_SURE_IO_THREAD(GetServiceByUuid, uuid,
                      BindToCurrentSequence(std::move(cb)));
  auto ret = GetServiceByUuidSync(uuid);
  EXEC_CB_AND_RET(cb, std::move(ret));
}

const bluetooth_v2_shlib::Addr& RemoteDeviceImpl::addr() const {
  return addr_;
}

void RemoteDeviceImpl::ReadCharacteristic(
    scoped_refptr<RemoteCharacteristicImpl> characteristic,
    bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
    RemoteCharacteristic::ReadCallback cb) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  handle_to_characteristic_read_cbs_[characteristic->handle()].push(
      std::move(cb));

  EnqueueOperation(
      __func__, base::BindOnce(&RemoteDeviceImpl::ReadCharacteristicImpl, this,
                               std::move(characteristic), auth_req));
}

void RemoteDeviceImpl::WriteCharacteristic(
    scoped_refptr<RemoteCharacteristicImpl> characteristic,
    bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
    bluetooth_v2_shlib::Gatt::WriteType write_type,
    std::vector<uint8_t> value,
    RemoteCharacteristic::StatusCallback cb) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  handle_to_characteristic_write_cbs_[characteristic->handle()].push(
      std::move(cb));
  EnqueueOperation(
      __func__, base::BindOnce(&RemoteDeviceImpl::WriteCharacteristicImpl, this,
                               std::move(characteristic), auth_req, write_type,
                               std::move(value)));
}

void RemoteDeviceImpl::ReadDescriptor(
    scoped_refptr<RemoteDescriptorImpl> descriptor,
    bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
    RemoteDescriptor::ReadCallback cb) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  handle_to_descriptor_read_cbs_[descriptor->handle()].push(std::move(cb));

  EnqueueOperation(__func__,
                   base::BindOnce(&RemoteDeviceImpl::ReadDescriptorImpl, this,
                                  std::move(descriptor), auth_req));
}

void RemoteDeviceImpl::WriteDescriptor(
    scoped_refptr<RemoteDescriptorImpl> descriptor,
    bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
    std::vector<uint8_t> value,
    RemoteDescriptor::StatusCallback cb) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  handle_to_descriptor_write_cbs_[descriptor->handle()].push(std::move(cb));
  EnqueueOperation(
      __func__,
      base::BindOnce(&RemoteDeviceImpl::WriteDescriptorImpl, this,
                     std::move(descriptor), auth_req, std::move(value)));
}

scoped_refptr<RemoteService> RemoteDeviceImpl::GetServiceByUuidSync(
    const bluetooth_v2_shlib::Uuid& uuid) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  auto it = uuid_to_service_.find(uuid);
  if (it == uuid_to_service_.end())
    return nullptr;

  return it->second;
}

void RemoteDeviceImpl::SetConnected(bool connected) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // We only set connected = true and call the callback after services are
  // discovered.
  if (!connected) {
    connected_ = false;
    ConnectComplete(false);
  }

  if (disconnect_pending_) {
    disconnect_pending_ = false;
    if (disconnect_cb_) {
      std::move(disconnect_cb_).Run(!connected);
    }
  }

  if (!connected && rssi_pending_) {
    LOG(ERROR) << "Read remote RSSI failed: disconnected";
    if (rssi_cb_) {
      std::move(rssi_cb_).Run(false, 0);
    }
    rssi_pending_ = false;
  }

  if (connected) {
    if (!gatt_client_manager_) {
      LOG(ERROR) << "Couldn't discover services: Destroyed";
      return;
    }

    if (!gatt_client_manager_->gatt_client()->GetServices(addr_)) {
      LOG(ERROR) << "Couldn't discover services, disconnecting";
      Disconnect({});
      ConnectComplete(false);
    }
  } else {
    // Reset state after disconnection
    mtu_ = kDefaultMtu;
    ClearServices();
  }
}

void RemoteDeviceImpl::SetBonded(bool bonded) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  bonded_ = bonded;

  if (create_bond_pending_) {
    create_bond_pending_ = false;
    if (create_bond_cb_) {
      std::move(create_bond_cb_).Run(bonded);
    }
  }

  if (remove_bond_pending_) {
    remove_bond_pending_ = false;
    if (remove_bond_cb_) {
      std::move(remove_bond_cb_).Run(!bonded);
    }
  }
}

void RemoteDeviceImpl::SetServicesDiscovered(bool discovered) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  services_discovered_ = discovered;
  if (!discovered) {
    return;
  }
  connected_ = true;
  ConnectComplete(true);
}

bool RemoteDeviceImpl::GetServicesDiscovered() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return services_discovered_;
}

void RemoteDeviceImpl::SetMtu(int mtu) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  mtu_ = mtu;
  if (!mtu_callbacks_.empty()) {
    std::move(mtu_callbacks_.front()).Run(true);
    mtu_callbacks_.pop();
    NotifyQueueOperationComplete();
  }
}

scoped_refptr<RemoteCharacteristic> RemoteDeviceImpl::CharacteristicFromHandle(
    uint16_t handle) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  auto it = handle_to_characteristic_.find(handle);
  if (it == handle_to_characteristic_.end())
    return nullptr;

  return it->second;
}

void RemoteDeviceImpl::OnCharacteristicRead(bool status,
                                            uint16_t handle,
                                            const std::vector<uint8_t>& value) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  auto it = handle_to_characteristic_read_cbs_.find(handle);
  if (it == handle_to_characteristic_read_cbs_.end() || it->second.empty()) {
    LOG(ERROR) << "No such characteristic read";
  } else {
    std::move(it->second.front()).Run(status, value);
    it->second.pop();
  }
  NotifyQueueOperationComplete();
}

void RemoteDeviceImpl::OnCharacteristicWrite(bool status, uint16_t handle) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  auto it = handle_to_characteristic_write_cbs_.find(handle);
  if (it == handle_to_characteristic_write_cbs_.end() || it->second.empty()) {
    LOG(ERROR) << "No such characteristic write";
  } else {
    std::move(it->second.front()).Run(status);
    it->second.pop();
  }
  NotifyQueueOperationComplete();
}

void RemoteDeviceImpl::OnDescriptorRead(bool status,
                                        uint16_t handle,
                                        const std::vector<uint8_t>& value) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  auto it = handle_to_descriptor_read_cbs_.find(handle);
  if (it == handle_to_descriptor_read_cbs_.end() || it->second.empty()) {
    LOG(ERROR) << "No such descriptor read";
  } else {
    std::move(it->second.front()).Run(status, value);
    it->second.pop();
  }
  NotifyQueueOperationComplete();
}

void RemoteDeviceImpl::OnDescriptorWrite(bool status, uint16_t handle) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  auto it = handle_to_descriptor_write_cbs_.find(handle);
  if (it == handle_to_descriptor_write_cbs_.end() || it->second.empty()) {
    LOG(ERROR) << "No such descriptor write";
  } else {
    std::move(it->second.front()).Run(status);
    it->second.pop();
  }
  NotifyQueueOperationComplete();
}

void RemoteDeviceImpl::OnGetServices(
    const std::vector<bluetooth_v2_shlib::Gatt::Service>& services) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  ClearServices();
  OnServicesAdded(services);
}

void RemoteDeviceImpl::OnServicesRemoved(uint16_t start_handle,
                                         uint16_t end_handle) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  for (auto it = uuid_to_service_.begin(); it != uuid_to_service_.end();) {
    if (it->second->handle() >= start_handle &&
        it->second->handle() <= end_handle) {
      for (auto& characteristic : it->second->GetCharacteristics()) {
        handle_to_characteristic_.erase(characteristic->handle());
      }
      it = uuid_to_service_.erase(it);
    } else {
      ++it;
    }
  }
}

void RemoteDeviceImpl::OnServicesAdded(
    const std::vector<bluetooth_v2_shlib::Gatt::Service>& services) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  for (const auto& service : services) {
    uuid_to_service_[service.uuid] = new RemoteServiceImpl(
        this, gatt_client_manager_, service, io_task_runner_);
  }

  for (const auto& pair : uuid_to_service_) {
    for (auto& characteristic : pair.second->GetCharacteristics()) {
      handle_to_characteristic_.emplace(
          characteristic->handle(),
          static_cast<RemoteCharacteristicImpl*>(characteristic.get()));
    }
  }
}

void RemoteDeviceImpl::OnReadRemoteRssiComplete(bool status, int rssi) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  rssi_pending_ = false;
  if (rssi_cb_) {
    std::move(rssi_cb_).Run(status, rssi);
  }
}

void RemoteDeviceImpl::ConnectComplete(bool success) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (connect_pending_) {
    connect_pending_ = false;
    if (connect_cb_) {
      std::move(connect_cb_).Run(success);
    }
  }
}

void RemoteDeviceImpl::EnqueueOperation(const std::string& name,
                                        base::OnceClosure op) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  command_queue_.emplace_back(name, std::move(op));

  // Run the operation if this is the only operation in the queue. Otherwise, it
  // will be executed when the current operation completes.
  if (command_queue_.size() == 1) {
    RunNextOperation();
  }
}

void RemoteDeviceImpl::NotifyQueueOperationComplete() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (command_queue_.empty()) {
    LOG(ERROR) << "Command queue is empty, device might be disconnected";
    return;
  }
  command_queue_.pop_front();
  command_timeout_timer_.Stop();

  // Run the next operation if there is one in the queue.
  if (!command_queue_.empty()) {
    RunNextOperation();
  }
}

void RemoteDeviceImpl::RunNextOperation() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (command_queue_.empty()) {
    LOG(ERROR) << "Command queue is empty, device might be disconnected";
    return;
  }
  auto& front = command_queue_.front();
  command_timeout_timer_.Start(
      FROM_HERE, kCommandTimeout,
      base::BindRepeating(&RemoteDeviceImpl::OnCommandTimeout, this,
                          front.first));
  std::move(front.second).Run();
}

void RemoteDeviceImpl::RequestMtuImpl(int mtu) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (gatt_client_manager_->gatt_client()->RequestMtu(addr_, mtu)) {
    return;
  }

  LOG(ERROR) << __func__ << " failed";
  DCHECK(!mtu_callbacks_.empty());
  std::move(mtu_callbacks_.front()).Run(false);
  mtu_callbacks_.pop();
  NotifyQueueOperationComplete();
}

void RemoteDeviceImpl::ReadCharacteristicImpl(
    scoped_refptr<RemoteCharacteristicImpl> characteristic,
    bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (gatt_client_manager_->gatt_client()->ReadCharacteristic(
          addr(), characteristic->characteristic(), auth_req)) {
    return;
  }

  LOG(ERROR) << __func__ << " failed";
  auto it = handle_to_characteristic_read_cbs_.find(characteristic->handle());
  DCHECK(it != handle_to_characteristic_read_cbs_.end());
  DCHECK(!it->second.empty());
  std::move(it->second.front()).Run(false, {});
  it->second.pop();
  NotifyQueueOperationComplete();
}

void RemoteDeviceImpl::WriteCharacteristicImpl(
    scoped_refptr<RemoteCharacteristicImpl> characteristic,
    bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
    bluetooth_v2_shlib::Gatt::WriteType write_type,
    std::vector<uint8_t> value) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (gatt_client_manager_->gatt_client()->WriteCharacteristic(
          addr(), characteristic->characteristic(), auth_req, write_type,
          value)) {
    return;
  }

  LOG(ERROR) << __func__ << " failed";
  auto it = handle_to_characteristic_write_cbs_.find(characteristic->handle());
  DCHECK(it != handle_to_characteristic_write_cbs_.end());
  DCHECK(!it->second.empty());
  std::move(it->second.front()).Run(false);
  it->second.pop();
  NotifyQueueOperationComplete();
}

void RemoteDeviceImpl::ReadDescriptorImpl(
    scoped_refptr<RemoteDescriptorImpl> descriptor,
    bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (gatt_client_manager_->gatt_client()->ReadDescriptor(
          addr(), descriptor->descriptor(), auth_req)) {
    return;
  }

  LOG(ERROR) << __func__ << " failed";
  auto it = handle_to_descriptor_read_cbs_.find(descriptor->handle());
  DCHECK(it != handle_to_descriptor_read_cbs_.end());
  DCHECK(!it->second.empty());
  std::move(it->second.front()).Run(false, {});
  it->second.pop();
  NotifyQueueOperationComplete();
}

void RemoteDeviceImpl::WriteDescriptorImpl(
    scoped_refptr<RemoteDescriptorImpl> descriptor,
    bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
    std::vector<uint8_t> value) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (gatt_client_manager_->gatt_client()->WriteDescriptor(
          addr(), descriptor->descriptor(), auth_req, value)) {
    return;
  }

  LOG(ERROR) << __func__ << " failed";
  auto it = handle_to_descriptor_write_cbs_.find(descriptor->handle());
  DCHECK(it != handle_to_descriptor_write_cbs_.end());
  DCHECK(!it->second.empty());
  std::move(it->second.front()).Run(false);
  it->second.pop();
  NotifyQueueOperationComplete();
}

void RemoteDeviceImpl::ClearServices() {
  for (auto& item : handle_to_characteristic_) {
    item.second->Invalidate();
  }

  uuid_to_service_.clear();
  handle_to_characteristic_.clear();
  command_queue_.clear();
  command_timeout_timer_.Stop();

  while (!mtu_callbacks_.empty()) {
    LOG(ERROR) << "RequestMtu failed: disconnected";
    std::move(mtu_callbacks_.front()).Run(false);
    mtu_callbacks_.pop();
  }

  for (auto& item : handle_to_characteristic_read_cbs_) {
    auto& queue = item.second;
    while (!queue.empty()) {
      LOG(ERROR) << "Characteristic read failed: disconnected";
      std::move(queue.front()).Run(false, {});
      queue.pop();
    }
  }
  handle_to_characteristic_read_cbs_.clear();

  for (auto& item : handle_to_characteristic_write_cbs_) {
    auto& queue = item.second;
    while (!queue.empty()) {
      LOG(ERROR) << "Characteristic write failed: disconnected";
      std::move(queue.front()).Run(false);
      queue.pop();
    }
  }
  handle_to_characteristic_write_cbs_.clear();

  for (auto& item : handle_to_descriptor_read_cbs_) {
    auto& queue = item.second;
    while (!queue.empty()) {
      LOG(ERROR) << "Descriptor read failed: disconnected";
      std::move(queue.front()).Run(false, {});
      queue.pop();
    }
  }
  handle_to_descriptor_read_cbs_.clear();

  for (auto& item : handle_to_descriptor_write_cbs_) {
    auto& queue = item.second;
    while (!queue.empty()) {
      LOG(ERROR) << "Descriptor write failed: disconnected";
      std::move(queue.front()).Run(false);
      queue.pop();
    }
  }
  handle_to_descriptor_write_cbs_.clear();
}

void RemoteDeviceImpl::OnCommandTimeout(const std::string& name) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  LOG(ERROR) << name << "(" << util::AddrLastByteString(addr_) << ")"
             << " timed out. Disconnecting";
  Disconnect(base::DoNothing());
}

}  // namespace bluetooth
}  // namespace chromecast
