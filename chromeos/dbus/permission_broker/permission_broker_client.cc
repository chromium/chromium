// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/permission_broker/permission_broker_client.h"

#include <stdint.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/permission_broker/fake_permission_broker_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using permission_broker::kCheckPathAccess;
using permission_broker::kClaimDevicePath;
using permission_broker::kDetachInterface;
using permission_broker::kOpenPath;
using permission_broker::kOpenPathAndRegisterClient;
using permission_broker::kPermissionBrokerInterface;
using permission_broker::kPermissionBrokerServiceName;
using permission_broker::kPermissionBrokerServicePath;
using permission_broker::kReattachInterface;
using permission_broker::kReleaseTcpPort;
using permission_broker::kReleaseTcpPortForward;
using permission_broker::kReleaseUdpPort;
using permission_broker::kReleaseUdpPortForward;
using permission_broker::kRequestTcpPortAccess;
using permission_broker::kRequestTcpPortForward;
using permission_broker::kRequestUdpPortAccess;
using permission_broker::kRequestUdpPortForward;

namespace chromeos {

namespace {

const char kNoResponseError[] = "org.chromium.Error.NoResponse";

PermissionBrokerClient* g_instance = nullptr;

}  // namespace

class PermissionBrokerClientImpl : public PermissionBrokerClient {
 public:
  PermissionBrokerClientImpl() = default;

  PermissionBrokerClientImpl(const PermissionBrokerClientImpl&) = delete;
  PermissionBrokerClientImpl& operator=(const PermissionBrokerClientImpl&) =
      delete;

  ~PermissionBrokerClientImpl() override = default;

  void CheckPathAccess(const std::string& path,
                       ResultCallback callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface, kCheckPathAccess);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(path);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PermissionBrokerClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void OpenPath(const std::string& path,
                OpenPathCallback callback,
                ErrorCallback error_callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface, kOpenPath);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(path);
    proxy_->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PermissionBrokerClientImpl::OnOpenPathResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&PermissionBrokerClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  void ClaimDevicePath(const std::string& path,
                       uint32_t allowed_interfaces_mask,
                       int lifeline_fd,
                       OpenPathCallback callback,
                       ErrorCallback error_callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface, kClaimDevicePath);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(path);
    writer.AppendUint32(allowed_interfaces_mask);
    writer.AppendFileDescriptor(lifeline_fd);
    proxy_->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PermissionBrokerClientImpl::OnOpenPathResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&PermissionBrokerClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  void OpenPathAndRegisterClient(const std::string& path,
                                 uint32_t allowed_interfaces_mask,
                                 int lifeline_fd,
                                 OpenPathAndRegisterClientCallback callback,
                                 ErrorCallback error_callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface,
                                 kOpenPathAndRegisterClient);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(path);
    writer.AppendUint32(allowed_interfaces_mask);
    writer.AppendFileDescriptor(lifeline_fd);
    proxy_->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(
            &PermissionBrokerClientImpl::OpenPathAndRegisterClientResponse,
            weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
        base::BindOnce(&PermissionBrokerClientImpl::OnError,
                       weak_ptr_factory_.GetWeakPtr(),
                       std::move(error_callback)));
  }

  void DetachInterface(const std::string& client_id,
                       uint8_t iface_num,
                       ResultCallback callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface, kDetachInterface);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(client_id);
    writer.AppendByte(iface_num);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PermissionBrokerClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ReattachInterface(const std::string& client_id,
                         uint8_t iface_num,
                         ResultCallback callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface,
                                 kReattachInterface);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(client_id);
    writer.AppendByte(iface_num);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PermissionBrokerClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RequestTcpPortAccess(uint16_t port,
                            const std::string& interface,
                            int lifeline_fd,
                            ResultCallback callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface,
                                 kRequestTcpPortAccess);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint16(port);
    writer.AppendString(interface);
    writer.AppendFileDescriptor(lifeline_fd);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PermissionBrokerClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RequestUdpPortAccess(uint16_t port,
                            const std::string& interface,
                            int lifeline_fd,
                            ResultCallback callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface,
                                 kRequestUdpPortAccess);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint16(port);
    writer.AppendString(interface);
    writer.AppendFileDescriptor(lifeline_fd);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PermissionBrokerClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ReleaseTcpPort(uint16_t port,
                      const std::string& interface,
                      ResultCallback callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface, kReleaseTcpPort);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint16(port);
    writer.AppendString(interface);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PermissionBrokerClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ReleaseUdpPort(uint16_t port,
                      const std::string& interface,
                      ResultCallback callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface, kReleaseUdpPort);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint16(port);
    writer.AppendString(interface);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PermissionBrokerClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RequestTcpPortForward(uint16_t in_port,
                             const std::string& in_interface,
                             const std::string& dst_ip,
                             uint16_t dst_port,
                             int lifeline_fd,
                             ResultCallback callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface,
                                 kRequestTcpPortForward);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint16(in_port);
    writer.AppendString(in_interface);
    writer.AppendString(dst_ip);
    writer.AppendUint16(dst_port);
    writer.AppendFileDescriptor(lifeline_fd);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PermissionBrokerClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void RequestUdpPortForward(uint16_t in_port,
                             const std::string& in_interface,
                             const std::string& dst_ip,
                             uint16_t dst_port,
                             int lifeline_fd,
                             ResultCallback callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface,
                                 kRequestUdpPortForward);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint16(in_port);
    writer.AppendString(in_interface);
    writer.AppendString(dst_ip);
    writer.AppendUint16(dst_port);
    writer.AppendFileDescriptor(lifeline_fd);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PermissionBrokerClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ReleaseTcpPortForward(uint16_t in_port,
                             const std::string& in_interface,
                             ResultCallback callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface,
                                 kReleaseTcpPortForward);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint16(in_port);
    writer.AppendString(in_interface);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PermissionBrokerClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void ReleaseUdpPortForward(uint16_t in_port,
                             const std::string& in_interface,
                             ResultCallback callback) override {
    dbus::MethodCall method_call(kPermissionBrokerInterface,
                                 kReleaseUdpPortForward);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint16(in_port);
    writer.AppendString(in_interface);
    proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PermissionBrokerClientImpl::OnResponse,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void Init(dbus::Bus* bus) {
    proxy_ =
        bus->GetObjectProxy(kPermissionBrokerServiceName,
                            dbus::ObjectPath(kPermissionBrokerServicePath));
  }

 private:
  // Handle a DBus response from the permission broker, invoking the callback
  // that the method was originally called with with the success response.
  void OnResponse(ResultCallback callback, dbus::Response* response) {
    if (!response) {
      LOG(WARNING) << "Access request method call failed.";
      std::move(callback).Run(false);
      return;
    }

    bool result = false;
    dbus::MessageReader reader(response);
    if (!reader.PopBool(&result))
      LOG(WARNING) << "Could not parse response: " << response->ToString();
    std::move(callback).Run(result);
  }

  void OnOpenPathResponse(OpenPathCallback callback, dbus::Response* response) {
    base::ScopedFD fd;
    dbus::MessageReader reader(response);
    if (!reader.PopFileDescriptor(&fd))
      LOG(WARNING) << "Could not parse response: " << response->ToString();
    std::move(callback).Run(std::move(fd));
  }

  void OpenPathAndRegisterClientResponse(
      OpenPathAndRegisterClientCallback callback,
      dbus::Response* response) {
    base::ScopedFD fd;
    std::string client_id;
    dbus::MessageReader reader(response);
    if (!reader.PopFileDescriptor(&fd)) {
      LOG(WARNING) << "Could not parse response for fd: "
                   << response->ToString();
    }
    if (!reader.PopString(&client_id)) {
      LOG(WARNING) << "Could not parse response for client_id: "
                   << response->ToString();
    }
    std::move(callback).Run(client_id, std::move(fd));
  }

  void OnError(ErrorCallback callback, dbus::ErrorResponse* response) {
    std::string error_name;
    std::string error_message;
    if (response) {
      dbus::MessageReader reader(response);
      error_name = response->GetErrorName();
      reader.PopString(&error_message);
    } else {
      error_name = kNoResponseError;
    }
    std::move(callback).Run(error_name, error_message);
  }

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;

  // Note: This should remain the last member so that it will be destroyed
  // first, invalidating its weak pointers, before the other members are
  // destroyed.
  base::WeakPtrFactory<PermissionBrokerClientImpl> weak_ptr_factory_{this};
};

PermissionBrokerClient::PermissionBrokerClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

PermissionBrokerClient::~PermissionBrokerClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void PermissionBrokerClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new PermissionBrokerClientImpl())->Init(bus);
}

// static
void PermissionBrokerClient::InitializeFake() {
  new FakePermissionBrokerClient();
}

// static
void PermissionBrokerClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
PermissionBrokerClient* PermissionBrokerClient::Get() {
  return g_instance;
}

}  // namespace chromeos
