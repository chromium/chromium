// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"

#include <ios>
#include <memory>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "net/base/ip_endpoint.h"

namespace ash {

ShillManagerClient::CreateP2PGroupParameter::CreateP2PGroupParameter(
    std::optional<std::string> ssid,
    std::optional<std::string> passphrase,
    const std::optional<uint32_t> frequency,
    const std::optional<shill::WiFiInterfacePriority> priority)
    : ssid(std::move(ssid)),
      passphrase(std::move(passphrase)),
      frequency(frequency),
      priority(priority) {}

ShillManagerClient::CreateP2PGroupParameter::CreateP2PGroupParameter(
    std::optional<std::string> ssid,
    std::optional<std::string> passphrase)
    : ssid(std::move(ssid)),
      passphrase(std::move(passphrase)),
      frequency(std::nullopt),
      priority(std::nullopt) {}

ShillManagerClient::CreateP2PGroupParameter::~CreateP2PGroupParameter() =
    default;

ShillManagerClient::ConnectP2PGroupParameter::ConnectP2PGroupParameter(
    std::string ssid,
    std::string passphrase,
    const std::optional<uint32_t> frequency,
    const std::optional<shill::WiFiInterfacePriority> priority)
    : ssid(std::move(ssid)),
      passphrase(std::move(passphrase)),
      frequency(frequency),
      priority(priority) {}

ShillManagerClient::ConnectP2PGroupParameter::~ConnectP2PGroupParameter() =
    default;

namespace {

ShillManagerClient* g_instance = nullptr;

// The ShillManagerClient implementation.
class ShillManagerClientImpl : public ShillManagerClient {
 public:
  ShillManagerClientImpl() = default;

  ShillManagerClientImpl(const ShillManagerClientImpl&) = delete;
  ShillManagerClientImpl& operator=(const ShillManagerClientImpl&) = delete;

  ~ShillManagerClientImpl() override = default;

  ////////////////////////////////////
  // ShillManagerClient overrides.
  void AddPropertyChangedObserver(
      ShillPropertyChangedObserver* observer) override {
    helper_->AddPropertyChangedObserver(observer);
  }

  void RemovePropertyChangedObserver(
      ShillPropertyChangedObserver* observer) override {
    helper_->RemovePropertyChangedObserver(observer);
  }

  void GetProperties(
      chromeos::DBusMethodCallback<base::Value::Dict> callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kGetPropertiesFunction);
    helper_->CallDictValueMethod(&method_call, std::move(callback));
  }

  void GetNetworksForGeolocation(
      chromeos::DBusMethodCallback<base::Value::Dict> callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kGetNetworksForGeolocation);
    helper_->CallDictValueMethod(&method_call, std::move(callback));
  }

  void SetProperty(const std::string& name,
                   const base::Value& value,
                   base::OnceClosure callback,
                   ErrorCallback error_callback) override {
    // This property is read-only and can only be mutated by the specialized
    // method exposed in DBus API.
    if (name == shill::kDNSProxyDOHProvidersProperty) {
      SetDNSProxyDOHProviders(value.GetDict(), std::move(callback),
                              std::move(error_callback));
      return;
    }

    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    ShillClientHelper::AppendValueDataAsVariant(&writer, name, value);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void RequestScan(const std::string& type,
                   base::OnceClosure callback,
                   ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kRequestScanFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void EnableTechnology(const std::string& type,
                        base::OnceClosure callback,
                        ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kEnableTechnologyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void SetNetworkThrottlingStatus(const NetworkThrottlingStatus& status,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kSetNetworkThrottlingFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(status.enabled);
    writer.AppendUint32(status.upload_rate_kbits);
    writer.AppendUint32(status.download_rate_kbits);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void DisableTechnology(const std::string& type,
                         base::OnceClosure callback,
                         ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kDisableTechnologyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void ConfigureService(const base::Value::Dict& properties,
                        chromeos::ObjectPathCallback callback,
                        ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kConfigureServiceFunction);
    dbus::MessageWriter writer(&method_call);
    ShillClientHelper::AppendServiceProperties(&writer, properties);
    helper_->CallObjectPathMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  void ConfigureServiceForProfile(const dbus::ObjectPath& profile_path,
                                  const base::Value::Dict& properties,
                                  chromeos::ObjectPathCallback callback,
                                  ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kConfigureServiceForProfileFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(dbus::ObjectPath(profile_path));
    ShillClientHelper::AppendServiceProperties(&writer, properties);
    helper_->CallObjectPathMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  void GetService(const base::Value::Dict& properties,
                  chromeos::ObjectPathCallback callback,
                  ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kGetServiceFunction);
    dbus::MessageWriter writer(&method_call);
    ShillClientHelper::AppendServiceProperties(&writer, properties);
    helper_->CallObjectPathMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  void ScanAndConnectToBestServices(base::OnceClosure callback,
                                    ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kScanAndConnectToBestServicesFunction);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void AddPasspointCredentials(const dbus::ObjectPath& profile_path,
                               const base::Value::Dict& properties,
                               base::OnceClosure callback,
                               ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kAddPasspointCredentialsFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(profile_path);
    ShillClientHelper::AppendServiceProperties(&writer, properties);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void RemovePasspointCredentials(const dbus::ObjectPath& profile_path,
                                  const base::Value::Dict& properties,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kRemovePasspointCredentialsFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(profile_path);
    ShillClientHelper::AppendServiceProperties(&writer, properties);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void SetTetheringEnabled(bool enabled,
                           StringCallback callback,
                           ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kSetTetheringEnabledFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(enabled);
    helper_->CallStringMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  void EnableTethering(const shill::WiFiInterfacePriority& priority,
                       StringCallback callback,
                       ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kEnableTetheringFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendUint32(static_cast<uint32_t>(priority));
    helper_->CallStringMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  void DisableTethering(StringCallback callback,
                        ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kDisableTetheringFunction);
    helper_->CallStringMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  void CheckTetheringReadiness(StringCallback callback,
                               ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kCheckTetheringReadinessFunction);
    helper_->CallStringMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  void SetLOHSEnabled(bool enabled,
                      base::OnceClosure callback,
                      ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kSetLOHSEnabledFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(enabled);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  void CreateP2PGroup(
      const CreateP2PGroupParameter& create_group_argument,
      base::OnceCallback<void(base::Value::Dict result)> callback,
      ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kCreateP2PGroupFunction);
    dbus::MessageWriter writer(&method_call);

    base::Value::Dict properties;
    const shill::WiFiInterfacePriority priority =
        create_group_argument.priority.has_value()
            ? create_group_argument.priority.value()
            : shill::WiFiInterfacePriority::FOREGROUND_WITH_FALLBACK;
    properties.Set(shill::kP2PDevicePriority, static_cast<int>(priority));
    if (create_group_argument.ssid.has_value()) {
      properties.Set(shill::kP2PDeviceSSID, create_group_argument.ssid.value());
    }

    if (create_group_argument.passphrase.has_value()) {
      properties.Set(shill::kP2PDevicePassphrase,
                     create_group_argument.passphrase.value());
    }

    if (create_group_argument.frequency.has_value()) {
      properties.Set(shill::kP2PDeviceFrequency,
                     static_cast<int>(create_group_argument.frequency.value()));
    }

    ShillClientHelper::AppendServiceProperties(&writer, properties);
    helper_->CallDictValueMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  void ConnectToP2PGroup(
      const ConnectP2PGroupParameter& connect_group_argument,
      base::OnceCallback<void(base::Value::Dict result)> callback,
      ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kConnectToP2PGroupFunction);
    dbus::MessageWriter writer(&method_call);

    base::Value::Dict properties;
    properties.Set(shill::kP2PDeviceSSID, connect_group_argument.ssid);
    properties.Set(shill::kP2PDevicePassphrase,
                   connect_group_argument.passphrase);
    const shill::WiFiInterfacePriority priority =
        connect_group_argument.priority.has_value()
            ? connect_group_argument.priority.value()
            : shill::WiFiInterfacePriority::FOREGROUND_WITH_FALLBACK;
    properties.Set(shill::kP2PDevicePriority, static_cast<int>(priority));
    if (connect_group_argument.frequency.has_value()) {
      properties.Set(
          shill::kP2PGroupInfoFrequencyProperty,
          static_cast<int>(connect_group_argument.frequency.value()));
    }

    ShillClientHelper::AppendServiceProperties(&writer, properties);
    helper_->CallDictValueMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  void DestroyP2PGroup(
      const int shill_id,
      base::OnceCallback<void(base::Value::Dict result)> callback,
      ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kDestroyP2PGroupFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(shill_id);
    helper_->CallDictValueMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  void DisconnectFromP2PGroup(
      const int shill_id,
      base::OnceCallback<void(base::Value::Dict result)> callback,
      ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kDisconnectFromP2PGroupFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendInt32(shill_id);
    helper_->CallDictValueMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  TestInterface* GetTestInterface() override { return nullptr; }

  void Init(dbus::Bus* bus) {
    proxy_ = bus->GetObjectProxy(shill::kFlimflamServiceName,
                                 dbus::ObjectPath(shill::kFlimflamServicePath));
    helper_ = std::make_unique<ShillClientHelper>(proxy_);
    helper_->MonitorPropertyChanged(shill::kFlimflamManagerInterface);
  }

 private:
  // Used by SetProperty call to reroute kDNSProxyDOHProviders to the underlying
  // specialized method in the DBus API.
  void SetDNSProxyDOHProviders(const base::Value::Dict& providers,
                               base::OnceClosure callback,
                               ErrorCallback error_callback) {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kSetDNSProxyDOHProvidersFunction);
    dbus::MessageWriter writer(&method_call);
    ShillClientHelper::AppendServiceProperties(&writer, providers);
    helper_->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                             std::move(error_callback));
  }

  raw_ptr<dbus::ObjectProxy> proxy_ = nullptr;
  std::unique_ptr<ShillClientHelper> helper_;
};

}  // namespace

ShillManagerClient::ShillManagerClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

ShillManagerClient::~ShillManagerClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ShillManagerClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  (new ShillManagerClientImpl)->Init(bus);
}

// static
void ShillManagerClient::InitializeFake() {
  new FakeShillManagerClient();
}

// static
void ShillManagerClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
ShillManagerClient* ShillManagerClient::Get() {
  return g_instance;
}

}  // namespace ash
