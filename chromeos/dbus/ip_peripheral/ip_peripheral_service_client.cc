// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ip_peripheral/ip_peripheral_service_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "chromeos/dbus/ip_peripheral/fake_ip_peripheral_service_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"

#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace {

IpPeripheralServiceClient* g_instance = nullptr;

void OnGetMethod(IpPeripheralServiceClient::GetCallback callback,
                 dbus::Response* response) {
  int32_t value = 0;
  int32_t min = 0;
  int32_t max = 0;

  if (!response) {
    LOG(ERROR) << "Unable to get pan/tilt/zoom value. Call failed, no response";
    std::move(callback).Run(false, value, min, max);
    return;
  }

  dbus::MessageReader reader(response);

  if (!reader.PopInt32(&value)) {
    LOG(ERROR) << "Unable to read pan/tilt/zoom value.";
    std::move(callback).Run(false, value, min, max);
    return;
  }

  if (!reader.PopInt32(&min)) {
    LOG(ERROR) << "Unable to read pan/tilt/zoom min value.";
    std::move(callback).Run(false, value, min, max);
    return;
  }

  if (!reader.PopInt32(&max)) {
    LOG(ERROR) << "Unable to read pan/tilt/zoom max value.";
    std::move(callback).Run(false, value, min, max);
    return;
  }

  std::move(callback).Run(true, value, min, max);
}

void OnSetMethod(IpPeripheralServiceClient::SetCallback callback,
                 dbus::Response* response) {
  if (!response) {
    LOG(ERROR) << "Unable to set pan/tilt/zoom value. Call failed, no response";
    std::move(callback).Run(false);
    return;
  }
  std::move(callback).Run(true);
}

// Real implementation of IpPeripheralServiceClient.
class IpPeripheralServiceClientImpl : public IpPeripheralServiceClient {
 public:
  IpPeripheralServiceClientImpl() = default;
  IpPeripheralServiceClientImpl(const IpPeripheralServiceClientImpl&) = delete;
  IpPeripheralServiceClientImpl& operator=(
      const IpPeripheralServiceClientImpl&) = delete;
  ~IpPeripheralServiceClientImpl() override = default;

  void GetPan(const std::string& ip, GetCallback callback) override {
    dbus::MethodCall method_call(ip_peripheral::kIpPeripheralServiceInterface,
                                 ip_peripheral::kGetPanMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(ip);
    ip_peripheral_service_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnGetMethod, std::move(callback)));
  }

  void GetTilt(const std::string& ip, GetCallback callback) override {
    dbus::MethodCall method_call(ip_peripheral::kIpPeripheralServiceInterface,
                                 ip_peripheral::kGetTiltMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(ip);
    ip_peripheral_service_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnGetMethod, std::move(callback)));
  }

  void GetZoom(const std::string& ip, GetCallback callback) override {
    dbus::MethodCall method_call(ip_peripheral::kIpPeripheralServiceInterface,
                                 ip_peripheral::kGetZoomMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(ip);
    ip_peripheral_service_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnGetMethod, std::move(callback)));
  }

  void SetPan(const std::string& ip,
              int32_t pan,
              SetCallback callback) override {
    dbus::MethodCall method_call(ip_peripheral::kIpPeripheralServiceInterface,
                                 ip_peripheral::kSetPanMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(ip);
    writer.AppendInt32(pan);
    ip_peripheral_service_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnSetMethod, std::move(callback)));
  }

  void SetTilt(const std::string& ip,
               int32_t tilt,
               SetCallback callback) override {
    dbus::MethodCall method_call(ip_peripheral::kIpPeripheralServiceInterface,
                                 ip_peripheral::kSetTiltMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(ip);
    writer.AppendInt32(tilt);
    ip_peripheral_service_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnSetMethod, std::move(callback)));
  }

  void SetZoom(const std::string& ip,
               int32_t zoom,
               SetCallback callback) override {
    dbus::MethodCall method_call(ip_peripheral::kIpPeripheralServiceInterface,
                                 ip_peripheral::kSetZoomMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(ip);
    writer.AppendInt32(zoom);
    ip_peripheral_service_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&OnSetMethod, std::move(callback)));
  }

  void Init(dbus::Bus* bus) {
    ip_peripheral_service_proxy_ = bus->GetObjectProxy(
        ip_peripheral::kIpPeripheralServiceName,
        dbus::ObjectPath(ip_peripheral::kIpPeripheralServicePath));
  }

 private:
  dbus::ObjectProxy* ip_peripheral_service_proxy_ = nullptr;
};

}  // namespace

IpPeripheralServiceClient::IpPeripheralServiceClient() {
  CHECK(!g_instance);
  g_instance = this;
}

IpPeripheralServiceClient::~IpPeripheralServiceClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void IpPeripheralServiceClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new IpPeripheralServiceClientImpl())->Init(bus);
}

// static
void IpPeripheralServiceClient::InitializeFake() {
  new FakeIpPeripheralServiceClient();
}

// static
void IpPeripheralServiceClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
IpPeripheralServiceClient* IpPeripheralServiceClient::Get() {
  return g_instance;
}

}  // namespace chromeos
