// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/gatt_client_manager_impl.h"

#include <deque>
#include <string>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/device/bluetooth/bluetooth_util.h"
#include "chromecast/device/bluetooth/le/remote_characteristic_impl.h"
#include "chromecast/device/bluetooth/le/remote_descriptor_impl.h"
#include "chromecast/device/bluetooth/le/remote_device_impl.h"
#include "chromecast/device/bluetooth/le/remote_service_impl.h"
#include "chromecast/public/bluetooth/gatt.h"

namespace chromecast {
namespace bluetooth {

namespace {

const int kMaxDevicesInQueue = 6;

#define RUN_ON_IO_THREAD(method, ...)                                       \
  io_task_runner_->PostTask(                                                \
      FROM_HERE, base::BindOnce(&GattClientManagerImpl::method, weak_this_, \
                                ##__VA_ARGS__));

#define MAKE_SURE_IO_THREAD(method, ...)            \
  DCHECK(io_task_runner_);                          \
  if (!io_task_runner_->BelongsToCurrentThread()) { \
    RUN_ON_IO_THREAD(method, ##__VA_ARGS__)         \
    return;                                         \
  }

#define CHECK_DEVICE_EXISTS_IT(it)                  \
  do {                                              \
    if (it == addr_to_device_.end()) {              \
      LOG(ERROR) << __func__ << ": No such device"; \
      return;                                       \
    }                                               \
  } while (0)

}  // namespace

// static
constexpr base::TimeDelta GattClientManagerImpl::kConnectTimeout;
constexpr base::TimeDelta GattClientManagerImpl::kDisconnectTimeout;
constexpr base::TimeDelta GattClientManagerImpl::kReadRemoteRssiTimeout;

// static
std::unique_ptr<GattClientManager> GattClientManager::Create(
    bluetooth_v2_shlib::GattClient* gatt_client,
    BluetoothManagerPlatform* bluetooth_manager,
    LeScanManager* le_scan_manager) {
  return std::make_unique<GattClientManagerImpl>(gatt_client);
}

GattClientManagerImpl::GattClientManagerImpl(
    bluetooth_v2_shlib::GattClient* gatt_client)
    : gatt_client_(gatt_client),
      observers_(new base::ObserverListThreadSafe<Observer>()),
      notification_logger_(this),
      weak_factory_(
          std::make_unique<base::WeakPtrFactory<GattClientManagerImpl>>(this)) {
  weak_this_ = weak_factory_->GetWeakPtr();
}

GattClientManagerImpl::~GattClientManagerImpl() {}

void GattClientManagerImpl::Initialize(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  io_task_runner_ = std::move(io_task_runner);
  InitializeOnIoThread();
}

void GattClientManagerImpl::InitializeOnIoThread() {
  MAKE_SURE_IO_THREAD(InitializeOnIoThread);
  gatt_client_->SetDelegate(this);
}

void GattClientManagerImpl::Finalize() {
  FinalizeOnIoThread();
}

void GattClientManagerImpl::AddObserver(Observer* o) {
  observers_->AddObserver(o);
}

void GattClientManagerImpl::RemoveObserver(Observer* o) {
  observers_->RemoveObserver(o);
}

void GattClientManagerImpl::GetDevice(
    const bluetooth_v2_shlib::Addr& addr,
    base::OnceCallback<void(scoped_refptr<RemoteDevice>)> cb) {
  MAKE_SURE_IO_THREAD(GetDevice, addr, BindToCurrentSequence(std::move(cb)));
  DCHECK(cb);
  std::move(cb).Run(GetDeviceSync(addr));
}

scoped_refptr<RemoteDevice> GattClientManagerImpl::GetDeviceSync(
    const bluetooth_v2_shlib::Addr& addr) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  auto it = addr_to_device_.find(addr);
  if (it != addr_to_device_.end()) {
    return it->second.get();
  }

  scoped_refptr<RemoteDeviceImpl> new_device(
      new RemoteDeviceImpl(addr, weak_this_, io_task_runner_));
  addr_to_device_[addr] = new_device;
  return new_device;
}

void GattClientManagerImpl::GetConnectedDevices(GetConnectDevicesCallback cb) {
  MAKE_SURE_IO_THREAD(GetConnectedDevices,
                      BindToCurrentSequence(std::move(cb)));
  std::vector<scoped_refptr<RemoteDevice>> devices;
  for (const auto& device : addr_to_device_) {
    if (device.second->IsConnected()) {
      devices.push_back(device.second);
    }
  }

  std::move(cb).Run(std::move(devices));
}

void GattClientManagerImpl::GetNumConnected(
    base::OnceCallback<void(size_t)> cb) const {
  MAKE_SURE_IO_THREAD(GetNumConnected, BindToCurrentSequence(std::move(cb)));
  DCHECK(cb);
  std::move(cb).Run(connected_devices_.size());
}

void GattClientManagerImpl::NotifyConnect(
    const bluetooth_v2_shlib::Addr& addr) {
  observers_->Notify(FROM_HERE, &Observer::OnConnectInitated, addr);
}

void GattClientManagerImpl::NotifyBonded(const bluetooth_v2_shlib::Addr& addr) {
  MAKE_SURE_IO_THREAD(NotifyBonded, addr);
  auto device = GetDeviceSync(addr);
  static_cast<RemoteDeviceImpl*>(device.get())->SetBonded(true);
  observers_->Notify(FROM_HERE, &Observer::OnBondChanged, device, true);
}

void GattClientManagerImpl::EnqueueConnectRequest(
    const bluetooth_v2_shlib::Addr& addr,
    bool is_connect,
    bluetooth_v2_shlib::Gatt::Client::Transport transport) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  pending_connect_requests_.emplace_back(addr, is_connect, transport);

  // Run the request if this is the only request in the queue. Otherwise, it
  // will be run when all previous requests complete.
  if (pending_connect_requests_.size() == 1) {
    RunQueuedConnectRequest();
  }
}

void GattClientManagerImpl::EnqueueReadRemoteRssiRequest(
    const bluetooth_v2_shlib::Addr& addr) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  // Get the last byte because whole address is PII.
  std::string addr_str = util::AddrLastByteString(addr);

  auto it = addr_to_device_.find(addr);
  if (it == addr_to_device_.end()) {
    LOG(ERROR) << "ReadRemoteRssi (" << addr_str << ") failed: no such device";
    return;
  }

  if (pending_read_remote_rssi_requests_.size() >= kMaxDevicesInQueue) {
    LOG(ERROR) << "ReadRemoteRssi (" << addr_str << ") failed: queue is full";
    it->second->OnReadRemoteRssiComplete(false, 0);
    return;
  }

  pending_read_remote_rssi_requests_.push_back(addr);

  // Run the request if this is the only request in the queue. Otherwise, it
  // will be run when all previous requests complete.
  if (pending_read_remote_rssi_requests_.size() == 1) {
    RunQueuedReadRemoteRssiRequest();
  }
}

bool GattClientManagerImpl::SetGattClientConnectable(bool connectable) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (gatt_client_connectable_ == connectable) {
    return false;
  }

  if (connectable) {
    if (disconnect_all_pending_) {
      LOG(ERROR) << "Can't enable GATT client connectability while "
                    "DisconectAll is pending";
      return false;
    }
    LOG(INFO) << "Enabling GATT client connectability";
  } else {
    LOG(INFO) << "Disabling GATT client connectability";
  }

  gatt_client_connectable_ = connectable;
  return true;
}

void GattClientManagerImpl::DisconnectAll(StatusCallback cb) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (disconnect_all_pending_) {
    LOG(ERROR) << "Already have a pending DisconectAll request";
    std::move(cb).Run(false);
    return;
  }

  if (connected_devices_.empty()) {
    std::move(cb).Run(true);
    return;
  }

  disconnect_all_pending_ = true;
  disconnect_all_cb_ = std::move(cb);
  for (const auto& addr : connected_devices_) {
    EnqueueConnectRequest(addr, false);
  }
}

bool GattClientManagerImpl::IsConnectedLeDevice(
    const bluetooth_v2_shlib::Addr& addr) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  return base::Contains(connected_devices_, addr);
}

scoped_refptr<base::SingleThreadTaskRunner>
GattClientManagerImpl::task_runner() {
  return io_task_runner_;
}

void GattClientManagerImpl::OnConnectChanged(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    bool connected) {
  MAKE_SURE_IO_THREAD(OnConnectChanged, addr, status, connected);
  auto it = addr_to_device_.find(addr);

  // Silently ignore devices we aren't keeping track of.
  if (it == addr_to_device_.end()) {
    return;
  }

  if (connected) {
    if (!gatt_client_connectable_) {
      LOG(ERROR) << "GATT client not connectable, disconnecting";
      gatt_client_->Disconnect(addr);
      return;
    }
    // We won't declare the device connected until service discovery completes,
    // so we won't start next Connect request until then.
    it->second->SetConnected(true);
    connected_devices_.insert(addr);
  } else {
    it->second->SetConnected(false);
    connected_devices_.erase(addr);
    if (!pending_connect_requests_.empty() &&
        addr == pending_connect_requests_.front().addr) {
      pending_connect_requests_.pop_front();
      connect_timeout_timer_.Stop();
      disconnect_timeout_timer_.Stop();
      RunQueuedConnectRequest();
    } else {
      std::erase_if(pending_connect_requests_,
                    [addr](const PendingRequest& request) {
                      return request.addr == addr;
                    });
    }

    std::erase(pending_read_remote_rssi_requests_, addr);
    read_remote_rssi_timeout_timer_.Stop();

    if (connected_devices_.empty()) {
      DisconnectAllComplete(true);
    }
  }

  // We won't declare the device connected until service discovery completes.
  // Only report disconnect callback if the connect callback was called (
  // service discovery completed).
  if (!connected && it->second->GetServicesDiscovered()) {
    it->second->SetServicesDiscovered(false);
    observers_->Notify(FROM_HERE, &Observer::OnConnectChanged, it->second,
                       false);
  }
}

void GattClientManagerImpl::OnBondChanged(const bluetooth_v2_shlib::Addr& addr,
                                          bool status,
                                          bool bonded) {
  MAKE_SURE_IO_THREAD(OnBondChanged, addr, status, bonded);
  auto it = addr_to_device_.find(addr);

  // Silently ignore devices we aren't keeping track of.
  if (it == addr_to_device_.end()) {
    return;
  }

  it->second->SetBonded(bonded);

  observers_->Notify(FROM_HERE, &Observer::OnBondChanged, it->second, bonded);
}

void GattClientManagerImpl::OnNotification(const bluetooth_v2_shlib::Addr& addr,
                                           uint16_t handle,
                                           const std::vector<uint8_t>& value) {
  MAKE_SURE_IO_THREAD(OnNotification, addr, handle, value);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  auto characteristic = it->second->CharacteristicFromHandle(handle);
  if (!characteristic) {
    LOG(ERROR) << "No such characteristic";
    return;
  }

  observers_->Notify(FROM_HERE, &Observer::OnCharacteristicNotification,
                     it->second, characteristic, value);
}

void GattClientManagerImpl::OnCharacteristicReadResponse(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    uint16_t handle,
    const std::vector<uint8_t>& value) {
  MAKE_SURE_IO_THREAD(OnCharacteristicReadResponse, addr, status, handle,
                      value);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnCharacteristicRead(status, handle, value);
}

void GattClientManagerImpl::OnCharacteristicWriteResponse(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    uint16_t handle) {
  MAKE_SURE_IO_THREAD(OnCharacteristicWriteResponse, addr, status, handle);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnCharacteristicWrite(status, handle);
}

void GattClientManagerImpl::OnDescriptorReadResponse(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    uint16_t handle,
    const std::vector<uint8_t>& value) {
  MAKE_SURE_IO_THREAD(OnDescriptorReadResponse, addr, status, handle, value);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnDescriptorRead(status, handle, value);
}

void GattClientManagerImpl::OnDescriptorWriteResponse(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    uint16_t handle) {
  MAKE_SURE_IO_THREAD(OnDescriptorWriteResponse, addr, status, handle);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnDescriptorWrite(status, handle);
}

void GattClientManagerImpl::OnReadRemoteRssi(
    const bluetooth_v2_shlib::Addr& addr,
    bool status,
    int rssi) {
  MAKE_SURE_IO_THREAD(OnReadRemoteRssi, addr, status, rssi);

  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnReadRemoteRssiComplete(status, rssi);

  if (pending_read_remote_rssi_requests_.empty() ||
      addr != pending_read_remote_rssi_requests_.front()) {
    // This can happen when the regular OnReadRemoteRssi is received after
    // ReadRemoteRssi timed out.
    LOG(ERROR) << "Unexpected call to " << __func__;
    return;
  }

  pending_read_remote_rssi_requests_.pop_front();
  read_remote_rssi_timeout_timer_.Stop();
  // Try to run the next ReadRemoteRssi request
  RunQueuedReadRemoteRssiRequest();
}

void GattClientManagerImpl::OnMtuChanged(const bluetooth_v2_shlib::Addr& addr,
                                         bool status,
                                         int mtu) {
  MAKE_SURE_IO_THREAD(OnMtuChanged, addr, status, mtu);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->SetMtu(mtu);

  observers_->Notify(FROM_HERE, &Observer::OnMtuChanged, it->second, mtu);
}

void GattClientManagerImpl::OnGetServices(
    const bluetooth_v2_shlib::Addr& addr,
    const std::vector<bluetooth_v2_shlib::Gatt::Service>& services) {
  MAKE_SURE_IO_THREAD(OnGetServices, addr, services);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnGetServices(services);

  if (!it->second->GetServicesDiscovered()) {
    it->second->SetServicesDiscovered(true);
    observers_->Notify(FROM_HERE, &Observer::OnConnectChanged, it->second,
                       true);
  }

  observers_->Notify(FROM_HERE, &Observer::OnServicesUpdated, it->second,
                     it->second->GetServicesSync());

  if (pending_connect_requests_.empty() ||
      addr != pending_connect_requests_.front().addr ||
      !pending_connect_requests_.front().is_connect) {
    NOTREACHED() << "Unexpected call to " << __func__;
  }

  pending_connect_requests_.pop_front();
  connect_timeout_timer_.Stop();
  // Try to run the next Connect request
  RunQueuedConnectRequest();
}

void GattClientManagerImpl::OnServicesRemoved(
    const bluetooth_v2_shlib::Addr& addr,
    uint16_t start_handle,
    uint16_t end_handle) {
  MAKE_SURE_IO_THREAD(OnServicesRemoved, addr, start_handle, end_handle);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnServicesRemoved(start_handle, end_handle);

  observers_->Notify(FROM_HERE, &Observer::OnServicesUpdated, it->second,
                     it->second->GetServicesSync());
}

void GattClientManagerImpl::OnServicesAdded(
    const bluetooth_v2_shlib::Addr& addr,
    const std::vector<bluetooth_v2_shlib::Gatt::Service>& services) {
  MAKE_SURE_IO_THREAD(OnServicesAdded, addr, services);
  auto it = addr_to_device_.find(addr);
  CHECK_DEVICE_EXISTS_IT(it);
  it->second->OnServicesAdded(services);
  observers_->Notify(FROM_HERE, &Observer::OnServicesUpdated, it->second,
                     it->second->GetServicesSync());
}

void GattClientManagerImpl::RunQueuedConnectRequest() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  while (!pending_connect_requests_.empty()) {
    const PendingRequest& pending_request = pending_connect_requests_.front();
    const bluetooth_v2_shlib::Addr& addr = pending_request.addr;
    const bool is_connect = pending_request.is_connect;
    if (is_connect) {
      if (gatt_client_connectable_) {
        if (gatt_client_->Connect(addr, pending_request.transport)) {
          connect_timeout_timer_.Start(
              FROM_HERE, kConnectTimeout,
              base::BindOnce(&GattClientManagerImpl::OnConnectTimeout,
                             weak_this_, addr));
          return;
        } else {
          LOG(ERROR) << "Connect failed";
          // Clear pending connect request to avoid device be in a bad state.
          gatt_client_->ClearPendingConnect(addr);
        }
      } else {
        LOG(ERROR) << "GATT client not connectable";
      }
      auto it = addr_to_device_.find(addr);
      if (it != addr_to_device_.end()) {
        it->second->SetConnected(false);
      }
    } else {
      if (gatt_client_->Disconnect(addr)) {
        disconnect_timeout_timer_.Start(
            FROM_HERE, kDisconnectTimeout,
            base::BindOnce(&GattClientManagerImpl::OnDisconnectTimeout,
                           weak_this_, addr));
        return;
      }
      LOG(ERROR) << "Disconnect failed";
      // Clear pending disconnect request to avoid device be in a bad state.
      gatt_client_->ClearPendingDisconnect(addr);

      auto it = addr_to_device_.find(addr);
      if (it != addr_to_device_.end()) {
        it->second->SetConnected(false);
      }

      DisconnectAllComplete(false);
    }

    // If current request fails, run the next request
    pending_connect_requests_.pop_front();
  }
}

void GattClientManagerImpl::RunQueuedReadRemoteRssiRequest() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (pending_read_remote_rssi_requests_.empty()) {
    return;
  }

  auto addr = pending_read_remote_rssi_requests_.front();
  while (!gatt_client_->ReadRemoteRssi(addr)) {
    // If current request fails, run the next request
    LOG(ERROR) << "ReadRemoteRssi failed";
    auto it = addr_to_device_.find(addr);
    if (it != addr_to_device_.end()) {
      it->second->OnReadRemoteRssiComplete(false, 0);
    }
    pending_read_remote_rssi_requests_.pop_front();

    if (pending_read_remote_rssi_requests_.empty()) {
      return;
    }

    addr = pending_read_remote_rssi_requests_.front();
  }

  read_remote_rssi_timeout_timer_.Start(
      FROM_HERE, kReadRemoteRssiTimeout,
      base::BindRepeating(&GattClientManagerImpl::OnReadRemoteRssiTimeout,
                          weak_this_, addr));
}

void GattClientManagerImpl::DisconnectAllComplete(bool success) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  if (!disconnect_all_pending_) {
    return;
  }
  disconnect_all_pending_ = false;

  if (disconnect_all_cb_) {
    std::move(disconnect_all_cb_).Run(success);
  }
}

void GattClientManagerImpl::OnConnectTimeout(
    const bluetooth_v2_shlib::Addr& addr) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // Get the last byte because whole address is PII.
  std::string addr_str = util::AddrLastByteString(addr);

  LOG(ERROR) << "Connect (" << addr_str << ")"
             << " timed out. Disconnecting";

  if (base::Contains(connected_devices_, addr)) {
    // Connect times out before OnGetServices is received.
    gatt_client_->Disconnect(addr);
  } else {
    // Connect times out before OnConnectChanged is received.
    gatt_client_->ClearPendingConnect(addr);
    RUN_ON_IO_THREAD(OnConnectChanged, addr, false /* status */,
                     false /* connected */);
  }
}

void GattClientManagerImpl::OnDisconnectTimeout(
    const bluetooth_v2_shlib::Addr& addr) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // Get the last byte because whole address is PII.
  std::string addr_str = util::AddrLastByteString(addr);

  LOG(ERROR) << "Disconnect (" << addr_str << ")"
             << " timed out.";

  gatt_client_->ClearPendingDisconnect(addr);
  DisconnectAllComplete(false);

  // Treat device as disconnected for this unknown case.
  RUN_ON_IO_THREAD(OnConnectChanged, addr, false /* status */,
                   false /* connected */);
}

void GattClientManagerImpl::OnReadRemoteRssiTimeout(
    const bluetooth_v2_shlib::Addr& addr) {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  // Get the last byte because whole address is PII.
  std::string addr_str = util::AddrLastByteString(addr);

  LOG(ERROR) << "ReadRemoteRssi (" << addr_str << ")"
             << " timed out.";

  // ReadRemoteRssi times out before OnReadRemoteRssi is received.
  RUN_ON_IO_THREAD(OnReadRemoteRssi, addr, false /* status */, 0 /* rssi */);
}

void GattClientManagerImpl::FinalizeOnIoThread() {
  MAKE_SURE_IO_THREAD(FinalizeOnIoThread);
  weak_factory_->InvalidateWeakPtrs();
  gatt_client_->SetDelegate(nullptr);
}

GattClientManagerImpl::PendingRequest::PendingRequest(
    const bluetooth_v2_shlib::Addr& addr,
    bool is_connect,
    bluetooth_v2_shlib::Gatt::Client::Transport transport)
    : addr(addr), is_connect(is_connect), transport(transport) {}

GattClientManagerImpl::PendingRequest::~PendingRequest() = default;

}  // namespace bluetooth
}  // namespace chromecast
