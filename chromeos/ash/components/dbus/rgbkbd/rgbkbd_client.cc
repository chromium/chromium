// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/rgbkbd/rgbkbd_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/dbus/rgbkbd/fake_rgbkbd_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/rgbkbd/dbus-constants.h"

namespace ash {

namespace {

RgbkbdClient* g_instance = nullptr;

class RgbkbdClientImpl : public RgbkbdClient {
 public:
  void Init(dbus::Bus* bus);
  RgbkbdClientImpl() = default;
  RgbkbdClientImpl(const RgbkbdClientImpl&) = delete;
  RgbkbdClientImpl& operator=(const RgbkbdClientImpl&) = delete;

  void GetRgbKeyboardCapabilities(
      GetRgbKeyboardCapabilitiesCallback callback) override {
    VLOG(1) << "rgbkbd: GetRgbKeyboardCapabilities called";
    dbus::MethodCall method_call(rgbkbd::kRgbkbdServiceName,
                                 rgbkbd::kGetRgbKeyboardCapabilities);
    dbus::MessageWriter writer(&method_call);
    CHECK(rgbkbd_proxy_);
    rgbkbd_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&RgbkbdClientImpl::GetRgbKeyboardCapabilitiesCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetCapsLockState(bool enabled) override {
    VLOG(1) << "rgbkbd: SetCapsLockState called with: "
            << (enabled ? "True" : "False");
    dbus::MethodCall method_call(rgbkbd::kRgbkbdServiceName,
                                 rgbkbd::kSetCapsLockState);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(enabled);
    CHECK(rgbkbd_proxy_);
    rgbkbd_proxy_->CallMethod(&method_call,
                              dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                              base::DoNothing());
  }

  void SetStaticBackgroundColor(uint8_t r, uint8_t g, uint8_t b) override {
    VLOG(1) << "rgbkbd: SetStaticBackgroundColor  R: " << static_cast<int>(r)
            << "G: " << static_cast<int>(g) << "B: " << static_cast<int>(b);
    dbus::MethodCall method_call(rgbkbd::kRgbkbdServiceName,
                                 rgbkbd::kSetStaticBackgroundColor);
    dbus::MessageWriter writer(&method_call);
    writer.AppendByte(r);
    writer.AppendByte(g);
    writer.AppendByte(b);
    CHECK(rgbkbd_proxy_);
    rgbkbd_proxy_->CallMethod(&method_call,
                              dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                              base::DoNothing());
  }

  void SetZoneColor(int zone, uint8_t r, uint8_t g, uint8_t b) override {
    VLOG(1) << "rgbkbd: SetZoneColor Zone: " << zone
            << "R: " << static_cast<int>(r) << "G: " << static_cast<int>(g)
            << "B: " << static_cast<int>(b);
    dbus::MethodCall method_call(rgbkbd::kRgbkbdServiceName,
                                 rgbkbd::kSetZoneColor);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(zone);
    writer.AppendByte(r);
    writer.AppendByte(g);
    writer.AppendByte(b);
    CHECK(rgbkbd_proxy_);
    rgbkbd_proxy_->CallMethod(&method_call,
                              dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                              base::DoNothing());
  }

  void SetRainbowMode() override {
    VLOG(1) << "rgbkbd: SetRainbowMode";
    dbus::MethodCall method_call(rgbkbd::kRgbkbdServiceName,
                                 rgbkbd::kSetRainbowMode);
    CHECK(rgbkbd_proxy_);
    rgbkbd_proxy_->CallMethod(&method_call,
                              dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                              base::DoNothing());
  }

  void SetAnimationMode(rgbkbd::RgbAnimationMode mode) override {
    VLOG(1) << "rgbkbd: SetAnimationMode with mode: "
            << static_cast<uint32_t>(mode);
    dbus::MethodCall method_call(rgbkbd::kRgbkbdServiceName,
                                 rgbkbd::kSetAnimationMode);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(static_cast<uint32_t>(mode));
    CHECK(rgbkbd_proxy_);
    rgbkbd_proxy_->CallMethod(&method_call,
                              dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                              base::DoNothing());
  }

 private:
  void GetRgbKeyboardCapabilitiesCallback(
      GetRgbKeyboardCapabilitiesCallback callback,
      dbus::Response* response) {
    if (!response) {
      VLOG(1)
          << "rgbkbd: No Dbus response received for GetRgbKeyboardCapabilities";
      std::move(callback).Run(std::nullopt);
      return;
    }
    dbus::MessageReader reader(response);
    uint32_t keyboard_capabilities;

    if (!reader.PopUint32(&keyboard_capabilities)) {
      LOG(ERROR)
          << "rgbkbd: Error reading GetRgbKeyboardCapabilities response: "
          << response->ToString();
      std::move(callback).Run(std::nullopt);
      return;
    }
    VLOG(1) << "rgbkbd: Value for keyboard capabilities is: "
            << keyboard_capabilities;
    std::move(callback).Run(
        rgbkbd::RgbKeyboardCapabilities(keyboard_capabilities));
  }

  void CapabilityUpdatedForTestingReceived(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    uint32_t capability;

    if (!reader.PopUint32(&capability)) {
      LOG(ERROR) << "rgbkbd: Error reading capability for testing response: "
                 << signal->ToString();
      return;
    }
    for (auto& observer : observers_) {
      observer.OnCapabilityUpdatedForTesting(  // IN-TEST
          rgbkbd::RgbKeyboardCapabilities(capability));
    }
  }

  void CapabilityUpdatedForTestingConnected(const std::string& interface_name,
                                            const std::string& signal_name,
                                            bool success) {
    LOG_IF(WARNING, !success)
        << "Failed to connect to CapabilityUpdatedForTesting signal.";
  }

  raw_ptr<dbus::ObjectProxy> rgbkbd_proxy_ = nullptr;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<RgbkbdClientImpl> weak_ptr_factory_{this};
};

}  // namespace

void RgbkbdClientImpl::Init(dbus::Bus* bus) {
  CHECK(bus);
  rgbkbd_proxy_ = bus->GetObjectProxy(
      rgbkbd::kRgbkbdServiceName, dbus::ObjectPath(rgbkbd::kRgbkbdServicePath));

  rgbkbd_proxy_->ConnectToSignal(
      rgbkbd::kRgbkbdServiceName, rgbkbd::kCapabilityUpdatedForTesting,
      base::BindRepeating(
          &RgbkbdClientImpl::CapabilityUpdatedForTestingReceived,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&RgbkbdClientImpl::CapabilityUpdatedForTestingConnected,
                     weak_ptr_factory_.GetWeakPtr()));
}

RgbkbdClient::RgbkbdClient() {
  CHECK(!g_instance);
  g_instance = this;
}

RgbkbdClient::~RgbkbdClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void RgbkbdClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new RgbkbdClientImpl())->Init(bus);
}

// static
void RgbkbdClient::InitializeFake() {
  new FakeRgbkbdClient();
}

// static
void RgbkbdClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
RgbkbdClient* RgbkbdClient::Get() {
  return g_instance;
}

void RgbkbdClient::AddObserver(RgbkbdClient::Observer* observer) {
  observers_.AddObserver(observer);
}

void RgbkbdClient::RemoveObserver(RgbkbdClient::Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
