// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_client.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "chromeos/ash/components/dbus/patchpanel/fake_patchpanel_client.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/patchpanel/dbus-constants.h"

namespace ash {
namespace {

PatchPanelClient* g_instance = nullptr;

void OnSignalConnected(const std::string& interface_name,
                       const std::string& signal_name,
                       bool success) {
  DCHECK_EQ(interface_name, patchpanel::kPatchPanelInterface);
  LOG_IF(DFATAL, !success) << "Failed to connect to D-Bus signal; interface: "
                           << interface_name << "; signal: " << signal_name;
}

// "Real" implementation of PatchPanelClient talking to the PatchPanel daemon
// on the Chrome OS side.
class PatchPanelClientImpl : public PatchPanelClient {
 public:
  PatchPanelClientImpl() = default;
  PatchPanelClientImpl(const PatchPanelClientImpl&) = delete;
  PatchPanelClientImpl& operator=(const PatchPanelClientImpl&) = delete;
  ~PatchPanelClientImpl() override = default;

  // PatchPanelClient overrides:
  void GetDevices(GetDevicesCallback callback) override {
    dbus::MethodCall method_call(patchpanel::kPatchPanelInterface,
                                 patchpanel::kGetDevicesMethod);
    dbus::MessageWriter writer(&method_call);

    patchpanel::GetDevicesRequest request;
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode GetDevicesRequest proto";
      return;
    }

    patchpanel_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PatchPanelClientImpl::HandleGetDevicesResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void Init(dbus::Bus* bus) override {
    patchpanel_proxy_ = bus->GetObjectProxy(
        patchpanel::kPatchPanelServiceName,
        dbus::ObjectPath(patchpanel::kPatchPanelServicePath));
    ConnectToSignals();
  }

  void AddObserver(Observer* observer) override {
    observer_list_.AddObserver(observer);
  }

  void RemoveObserver(Observer* observer) override {
    observer_list_.RemoveObserver(observer);
  }

 private:
  void HandleGetDevicesResponse(GetDevicesCallback callback,
                                dbus::Response* dbus_response) {
    patchpanel::GetDevicesResponse response;
    dbus::MessageReader reader(dbus_response);
    if (!reader.PopArrayOfBytesAsProto(&response)) {
      LOG(ERROR) << "Failed to parse GetDevices response proto";
      std::move(callback).Run({});
      return;
    }
    std::move(callback).Run(std::vector<patchpanel::NetworkDevice>(
        std::make_move_iterator(response.devices().begin()),
        std::make_move_iterator(response.devices().end())));
  }

  void OnNetworkConfigurationChanged(dbus::Signal* signal) {
    for (auto& observer : observer_list_) {
      observer.NetworkConfigurationChanged();
    }
  }

  // Connects the dbus signals.
  void ConnectToSignals() {
    patchpanel_proxy_->ConnectToSignal(
        patchpanel::kPatchPanelInterface,
        patchpanel::kNetworkConfigurationChangedSignal,
        base::BindRepeating(
            &PatchPanelClientImpl::OnNetworkConfigurationChanged,
            weak_factory_.GetWeakPtr()),
        base::BindOnce(&OnSignalConnected));
  }

  // D-Bus proxy for the PatchPanel daemon, not owned.
  raw_ptr<dbus::ObjectProxy, ExperimentalAsh> patchpanel_proxy_ = nullptr;

  // List of observers for dbus signals.
  base::ObserverList<Observer> observer_list_;

  base::WeakPtrFactory<PatchPanelClientImpl> weak_factory_{this};
};

}  // namespace

PatchPanelClient::PatchPanelClient() {
  CHECK(!g_instance);
  g_instance = this;
}

PatchPanelClient::~PatchPanelClient() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void PatchPanelClient::Initialize(dbus::Bus* bus) {
  CHECK(bus);
  (new PatchPanelClientImpl())->Init(bus);
}

// static
void PatchPanelClient::InitializeFake() {
  new FakePatchPanelClient();
}

// static
void PatchPanelClient::Shutdown() {
  CHECK(g_instance);
  delete g_instance;
}

// static
PatchPanelClient* PatchPanelClient::Get() {
  return g_instance;
}

}  // namespace ash
