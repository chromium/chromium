// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/pciguard/pciguard_client.h"

#include "base/callback_helpers.h"
#include "chromeos/dbus/pciguard/fake_pciguard_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/pciguard/dbus-constants.h"

namespace chromeos {

namespace {
PciguardClient* g_instance = nullptr;
}  // namespace

class PciguardClientImpl : public PciguardClient {
 public:
  PciguardClientImpl() = default;
  PciguardClientImpl(const PciguardClientImpl&) = delete;
  PciguardClientImpl& operator=(const PciguardClientImpl&) = delete;
  ~PciguardClientImpl() override = default;

  // PciguardClient:: overrides
  void SendExternalPciDevicesPermissionState(bool permitted) override;

  void Init(dbus::Bus* bus) {
    pci_guard_proxy_ =
        bus->GetObjectProxy(pciguard::kPciguardServiceName,
                            dbus::ObjectPath(pciguard::kPciguardServicePath));
  }

 private:
  dbus::ObjectProxy* pci_guard_proxy_ = nullptr;
};

PciguardClient::PciguardClient() {
  CHECK(!g_instance);
  g_instance = this;
}

PciguardClient::~PciguardClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// PciguardClientImpl
void PciguardClientImpl::SendExternalPciDevicesPermissionState(bool permitted) {
  dbus::MethodCall method_call(
      pciguard::kPciguardServiceInterface,
      pciguard::kSetExternalPciDevicesPermissionMethod);
  dbus::MessageWriter writer(&method_call);
  writer.AppendBool(permitted);

  pci_guard_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, base::DoNothing());
}

// static
void PciguardClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new PciguardClientImpl())->Init(bus);
}

// static
void PciguardClient::InitializeFake() {
  new FakePciguardClient();
}

// static
void PciguardClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
PciguardClient* PciguardClient::Get() {
  return g_instance;
}

}  // namespace chromeos
