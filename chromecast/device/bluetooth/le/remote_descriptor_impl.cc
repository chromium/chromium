// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/device/bluetooth/le/remote_descriptor_impl.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "chromecast/base/bind_to_task_runner.h"
#include "chromecast/device/bluetooth/le/gatt_client_manager_impl.h"
#include "chromecast/device/bluetooth/le/remote_device_impl.h"

#define EXEC_CB_AND_RET(cb, ret, ...)        \
  do {                                       \
    if (cb) {                                \
      std::move(cb).Run(ret, ##__VA_ARGS__); \
    }                                        \
    return;                                  \
  } while (0)

#define RUN_ON_IO_THREAD(method, ...) \
  io_task_runner_->PostTask(          \
      FROM_HERE,                      \
      base::BindOnce(&RemoteDescriptorImpl::method, this, ##__VA_ARGS__));

#define MAKE_SURE_IO_THREAD(method, ...)            \
  DCHECK(io_task_runner_);                          \
  if (!io_task_runner_->BelongsToCurrentThread()) { \
    RUN_ON_IO_THREAD(method, ##__VA_ARGS__)         \
    return;                                         \
  }

namespace chromecast {
namespace bluetooth {

RemoteDescriptorImpl::RemoteDescriptorImpl(
    RemoteDeviceImpl* device,
    base::WeakPtr<GattClientManagerImpl> gatt_client_manager,
    const bluetooth_v2_shlib::Gatt::Descriptor* descriptor,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner)
    : device_(device),
      gatt_client_manager_(std::move(gatt_client_manager)),
      descriptor_(descriptor),
      io_task_runner_(std::move(io_task_runner)) {
  DCHECK(device);
  DCHECK(gatt_client_manager_);
  DCHECK(descriptor);
  DCHECK(io_task_runner_->BelongsToCurrentThread());
}

RemoteDescriptorImpl::~RemoteDescriptorImpl() = default;

void RemoteDescriptorImpl::ReadAuth(
    bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
    ReadCallback callback) {
  MAKE_SURE_IO_THREAD(ReadAuth, auth_req,
                      BindToCurrentSequence(std::move(callback)));
  DCHECK(callback);
  if (!gatt_client_manager_) {
    LOG(ERROR) << __func__ << " failed: Destroyed";
    EXEC_CB_AND_RET(callback, false, {});
  }

  device_->ReadDescriptor(this, auth_req, std::move(callback));
}

void RemoteDescriptorImpl::Read(ReadCallback callback) {
  ReadAuth(bluetooth_v2_shlib::Gatt::Client::AUTH_REQ_INVALID,
           std::move(callback));
}

void RemoteDescriptorImpl::WriteAuth(
    bluetooth_v2_shlib::Gatt::Client::AuthReq auth_req,
    const std::vector<uint8_t>& value,
    StatusCallback callback) {
  MAKE_SURE_IO_THREAD(WriteAuth, auth_req, value,
                      BindToCurrentSequence(std::move(callback)));
  DCHECK(callback);
  if (!gatt_client_manager_) {
    LOG(ERROR) << __func__ << " failed: Destroyed";
    EXEC_CB_AND_RET(callback, false);
  }

  device_->WriteDescriptor(this, auth_req, value, std::move(callback));
}

void RemoteDescriptorImpl::Write(const std::vector<uint8_t>& value,
                                 StatusCallback callback) {
  WriteAuth(bluetooth_v2_shlib::Gatt::Client::AUTH_REQ_INVALID, value,
            std::move(callback));
}

const bluetooth_v2_shlib::Gatt::Descriptor& RemoteDescriptorImpl::descriptor()
    const {
  return *descriptor_;
}

const bluetooth_v2_shlib::Uuid RemoteDescriptorImpl::uuid() const {
  return descriptor_->uuid;
}

HandleId RemoteDescriptorImpl::handle() const {
  return descriptor_->handle;
}

bluetooth_v2_shlib::Gatt::Permissions RemoteDescriptorImpl::permissions()
    const {
  return descriptor_->permissions;
}

void RemoteDescriptorImpl::Invalidate() {
  DCHECK(io_task_runner_->BelongsToCurrentThread());
  gatt_client_manager_.reset();
}

}  // namespace bluetooth
}  // namespace chromecast
