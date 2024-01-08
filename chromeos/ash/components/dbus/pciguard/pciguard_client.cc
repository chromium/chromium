// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/pciguard/pciguard_client.h"

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/pciguard/fake_pciguard_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/pciguard/dbus-constants.h"

namespace ash {

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

  void Init(dbus::Bus* bus);

 private:
  void BlockedThunderboltDevicedConnectedReceieved(dbus::Signal* signal);
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success);

  raw_ptr<dbus::ObjectProxy> pci_guard_proxy_ = nullptr;
  base::WeakPtrFactory<PciguardClientImpl> weak_ptr_factory_{this};
};

// PciguardClientImpl
void PciguardClientImpl::Init(dbus::Bus* bus) {
  pci_guard_proxy_ =
      bus->GetObjectProxy(pciguard::kPciguardServiceName,
                          dbus::ObjectPath(pciguard::kPciguardServicePath));

  // Listen to D-Bus signals emitted by pciguard.
  typedef void (PciguardClientImpl::*SignalMethod)(dbus::Signal*);
  const std::pair<const char*, SignalMethod> kSignalMethods[] = {
      {pciguard::kPCIDeviceBlockedSignal,
       &PciguardClientImpl::BlockedThunderboltDevicedConnectedReceieved}};

  auto on_connected_callback = base::BindRepeating(
      &PciguardClientImpl::OnSignalConnected, weak_ptr_factory_.GetWeakPtr());

  for (const auto& signal : kSignalMethods) {
    pci_guard_proxy_->ConnectToSignal(
        pciguard::kPciguardServiceInterface, signal.first,
        base::BindRepeating(signal.second, weak_ptr_factory_.GetWeakPtr()),
        on_connected_callback);
  }
}

void PciguardClientImpl::BlockedThunderboltDevicedConnectedReceieved(
    dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  std::string device_name;
  if (!reader.PopString(&device_name)) {
    LOG(ERROR) << "Pciguard: Unable to decode connected device name from "
               << pciguard::kPCIDeviceBlockedSignal << " signal.";
    return;
  }

  VLOG(1) << "Pciguard: Received blocked device: " << device_name;
  NotifyOnBlockedThunderboltDeviceConnected(device_name);
}

void PciguardClientImpl::OnSignalConnected(const std::string& interface_name,
                                           const std::string& signal_name,
                                           bool success) {
  if (!success) {
    LOG(ERROR) << "Pciguard: Failed to connect to signal " << signal_name
               << ".";
    return;
  }
  VLOG(1) << "Pciguard: Successfully connected to signal " << signal_name
          << ".";
}

void PciguardClientImpl::SendExternalPciDevicesPermissionState(bool permitted) {
  dbus::MethodCall method_call(
      pciguard::kPciguardServiceInterface,
      pciguard::kSetExternalPciDevicesPermissionMethod);
  VLOG(1) << "Pciguard: Sending data access enabled state: " << permitted;

  dbus::MessageWriter writer(&method_call);
  writer.AppendBool(permitted);

  pci_guard_proxy_->CallMethod(
      &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT, base::DoNothing());
}

// PciguardClient
PciguardClient::PciguardClient() {
  CHECK(!g_instance);
  g_instance = this;
}

PciguardClient::~PciguardClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void PciguardClient::AddObserver(PciguardClient::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void PciguardClient::RemoveObserver(PciguardClient::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void PciguardClient::NotifyOnBlockedThunderboltDeviceConnected(
    const std::string& device_name) {
  for (auto& observer : observer_list_)
    observer.OnBlockedThunderboltDeviceConnected(device_name);
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

}  // namespace ash
