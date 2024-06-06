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
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/patchpanel/fake_patchpanel_client.h"
#include "chromeos/ash/components/dbus/patchpanel/patchpanel_service.pb.h"
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

  void NotifyAndroidInteractiveState(bool interactive) override {
    dbus::MethodCall method_call(
        patchpanel::kPatchPanelInterface,
        patchpanel::kNotifyAndroidInteractiveStateMethod);
    dbus::MessageWriter writer(&method_call);

    patchpanel::NotifyAndroidInteractiveStateRequest request;
    request.set_interactive(interactive);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to encode NotifyAndroidInteractiveState proto";
      return;
    }

    patchpanel_proxy_->CallMethod(&method_call,
                                  dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                  base::DoNothing());
  }

  void NotifyAndroidWifiMulticastLockChange(bool is_held) override {
    dbus::MethodCall method_call(
        patchpanel::kPatchPanelInterface,
        patchpanel::kNotifyAndroidWifiMulticastLockChangeMethod);
    dbus::MessageWriter writer(&method_call);

    patchpanel::NotifyAndroidWifiMulticastLockChangeRequest request;
    request.set_held(is_held);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to serialize NotifyAndroidWifiMulticastLockChange "
                    "request proto";
      return;
    }

    patchpanel_proxy_->CallMethod(&method_call,
                                  dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                  base::DoNothing());
  }

  void NotifySocketConnectionEvent(
      const patchpanel::SocketConnectionEvent& msg) override {
    dbus::MethodCall method_call(
        patchpanel::kPatchPanelInterface,
        patchpanel::kNotifySocketConnectionEventMethod);
    dbus::MessageWriter writer(&method_call);

    patchpanel::NotifySocketConnectionEventRequest request;
    *request.mutable_msg() = msg;

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to serialize NotifySocketConnectionEvent "
                    "request proto";
      return;
    }

    patchpanel_proxy_->CallMethod(&method_call,
                                  dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                  base::DoNothing());
  }

  void NotifyARCVPNSocketConnectionEvent(
      const patchpanel::SocketConnectionEvent& msg) override {
    dbus::MethodCall method_call(
        patchpanel::kPatchPanelInterface,
        patchpanel::kNotifyARCVPNSocketConnectionEventMethod);
    dbus::MessageWriter writer(&method_call);

    patchpanel::NotifyARCVPNSocketConnectionEventRequest request;
    *request.mutable_msg() = msg;

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to serialize NotifyARCVPNSocketConnectionEvent "
                    "request proto";
      return;
    }

    patchpanel_proxy_->CallMethod(&method_call,
                                  dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                  base::DoNothing());
  }

  void TagSocket(int socket_fd,
                 std::optional<int> network_id,
                 std::optional<VpnRoutingPolicy> vpn_policy,
                 TagSocketCallback callback) override {
    using patchpanel::TagSocketRequest;

    dbus::MethodCall method_call(patchpanel::kPatchPanelInterface,
                                 patchpanel::kTagSocketMethod);
    dbus::MessageWriter writer(&method_call);

    TagSocketRequest::VpnRoutingPolicy proto_vpn_policy =
        TagSocketRequest::DEFAULT_ROUTING;
    if (vpn_policy.has_value()) {
      switch (*vpn_policy) {
        case VpnRoutingPolicy::kRouteOnVpn:
          proto_vpn_policy = TagSocketRequest::ROUTE_ON_VPN;
          break;
        case VpnRoutingPolicy::kBypassVpn:
          proto_vpn_policy = TagSocketRequest::BYPASS_VPN;
          break;
      }
    }

    TagSocketRequest request;
    if (network_id.has_value()) {
      request.set_network_id(*network_id);
    }
    request.set_vpn_policy(proto_vpn_policy);

    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to serialize TagSocketRequest proto";
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), /*success=*/false));
      return;
    }
    writer.AppendFileDescriptor(socket_fd);

    patchpanel_proxy_->CallMethod(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&PatchPanelClientImpl::HandleTagSocketResponse,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
  }

  void SetFeatureFlag(patchpanel::SetFeatureFlagRequest::FeatureFlag flag,
                      bool enabled) override {
    // SetFeatureFlag is be called on Chrome starts and it's very likely that
    // patchpanel D-Bus service is not ready at that time, so we want to use
    // WaitForServiceToBeAvailable() to post the task here. Note that if the
    // service is already ready, it will behave similar to a PostTask() so this
    // won't be harmful here.
    patchpanel_proxy_->WaitForServiceToBeAvailable(
        base::BindOnce(&PatchPanelClientImpl::SetFeatureFlagImpl,
                       weak_factory_.GetWeakPtr(), flag, enabled));
  }

  void SetFeatureFlagImpl(patchpanel::SetFeatureFlagRequest::FeatureFlag flag,
                          bool enabled,
                          bool patchpanel_service_ready) {
    if (!patchpanel_service_ready) {
      LOG(ERROR) << "Patchpanel service is not ready";
      return;
    }

    dbus::MethodCall method_call(patchpanel::kPatchPanelInterface,
                                 patchpanel::kSetFeatureFlagMethod);
    dbus::MessageWriter writer(&method_call);

    patchpanel::SetFeatureFlagRequest request;
    request.set_flag(flag);
    request.set_enabled(enabled);
    if (!writer.AppendProtoAsArrayOfBytes(request)) {
      LOG(ERROR) << "Failed to serialize SetFeatureFlag "
                    "request proto";
      return;
    }

    patchpanel_proxy_->CallMethod(&method_call,
                                  dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
                                  base::DoNothing());
  }

  void Init(dbus::Bus* bus) override {
    patchpanel_proxy_ = bus->GetObjectProxy(
        patchpanel::kPatchPanelServiceName,
        dbus::ObjectPath(patchpanel::kPatchPanelServicePath));
    ConnectToSignals();
  }

  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback)
      override {
    patchpanel_proxy_->WaitForServiceToBeAvailable(std::move(callback));
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

  void HandleTagSocketResponse(TagSocketCallback callback,
                               dbus::Response* dbus_response) {
    patchpanel::TagSocketResponse response;
    dbus::MessageReader reader(dbus_response);
    if (!reader.PopArrayOfBytesAsProto(&response)) {
      LOG(ERROR) << "Failed to parse TagSocketResponse proto";
      std::move(callback).Run({});
      return;
    }
    std::move(callback).Run(response.success());
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
  raw_ptr<dbus::ObjectProxy> patchpanel_proxy_ = nullptr;

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
