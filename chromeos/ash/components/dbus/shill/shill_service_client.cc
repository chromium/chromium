// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/shill_service_client.h"

#include <map>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_service_client.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

#ifndef DBUS_ERROR_UNKNOWN_OBJECT
// The linux_chromeos ASAN builder has an older version of dbus-protocol.h
// so make sure this is defined.
#define DBUS_ERROR_UNKNOWN_OBJECT "org.freedesktop.DBus.Error.UnknownObject"
#endif

const char kInvalidPathError[] = "InvalidObjectPath";

ShillServiceClient* g_instance = nullptr;

// Error callback for GetProperties.
void OnGetDictionaryError(
    const std::string& method_name,
    const dbus::ObjectPath& service_path,
    chromeos::DBusMethodCallback<base::Value::Dict> callback,
    const std::string& error_name,
    const std::string& error_message) {
  const std::string log_string = "Failed to call org.chromium.shill.Service." +
                                 method_name + " for: " + service_path.value() +
                                 ": " + error_name + ": " + error_message;

  // Suppress ERROR messages for UnknownMethod/Object" since this can
  // happen under normal conditions. See crbug.com/130660 and crbug.com/222210.
  if (error_name == DBUS_ERROR_UNKNOWN_METHOD ||
      error_name == DBUS_ERROR_UNKNOWN_OBJECT) {
    VLOG(1) << log_string;
  } else {
    LOG(ERROR) << log_string;
  }

  std::move(callback).Run(std::nullopt);
}

// The ShillServiceClient implementation.
class ShillServiceClientImpl : public ShillServiceClient {
 public:
  explicit ShillServiceClientImpl(dbus::Bus* bus) : bus_(bus) {}

  ShillServiceClientImpl(const ShillServiceClientImpl&) = delete;
  ShillServiceClientImpl& operator=(const ShillServiceClientImpl&) = delete;

  ~ShillServiceClientImpl() override {
    for (auto& helper_pair : helpers_) {
      ShillClientHelper* helper = helper_pair.second;
      bus_->RemoveObjectProxy(shill::kFlimflamServiceName,
                              helper->object_proxy()->object_path(),
                              base::DoNothing());
      delete helper;
    }
  }

  void AddPropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      return;
    }
    helper->AddPropertyChangedObserver(observer);
  }

  void RemovePropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      return;
    }
    helper->RemovePropertyChangedObserver(observer);
  }

  void GetProperties(
      const dbus::ObjectPath& service_path,
      chromeos::DBusMethodCallback<base::Value::Dict> callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(callback).Run(base::Value::Dict());
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kGetPropertiesFunction);
    auto split_callback = base::SplitOnceCallback(std::move(callback));
    helper->CallDictValueMethodWithErrorCallback(
        &method_call,
        AdaptCallbackWithoutStatus(std::move(split_callback.first)),
        base::BindOnce(&OnGetDictionaryError, "GetProperties", service_path,
                       std::move(split_callback.second)));
  }

  void SetProperty(const dbus::ObjectPath& service_path,
                   const std::string& name,
                   const base::Value& value,
                   base::OnceClosure callback,
                   ErrorCallback error_callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(error_callback).Run(kInvalidPathError, "");
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    ShillClientHelper::AppendValueDataAsVariant(&writer, name, value);
    helper->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                            std::move(error_callback));
  }

  void SetProperties(const dbus::ObjectPath& service_path,
                     const base::Value::Dict& properties,
                     base::OnceClosure callback,
                     ErrorCallback error_callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(error_callback).Run(kInvalidPathError, "");
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kSetPropertiesFunction);
    dbus::MessageWriter writer(&method_call);
    ShillClientHelper::AppendServiceProperties(&writer, properties);
    helper->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                            std::move(error_callback));
  }

  void ClearProperty(const dbus::ObjectPath& service_path,
                     const std::string& name,
                     base::OnceClosure callback,
                     ErrorCallback error_callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(error_callback).Run(kInvalidPathError, "");
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kClearPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    helper->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                            std::move(error_callback));
  }

  void ClearProperties(const dbus::ObjectPath& service_path,
                       const std::vector<std::string>& names,
                       ListValueCallback callback,
                       ErrorCallback error_callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(error_callback).Run(kInvalidPathError, "");
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kClearPropertiesFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendArrayOfStrings(names);
    helper->CallListValueMethodWithErrorCallback(
        &method_call, std::move(callback), std::move(error_callback));
  }

  void Connect(const dbus::ObjectPath& service_path,
               base::OnceClosure callback,
               ErrorCallback error_callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(error_callback).Run(kInvalidPathError, "");
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kConnectFunction);
    helper->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                            std::move(error_callback));
  }

  void Disconnect(const dbus::ObjectPath& service_path,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(error_callback).Run(kInvalidPathError, "");
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kDisconnectFunction);
    helper->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                            std::move(error_callback));
  }

  void Remove(const dbus::ObjectPath& service_path,
              base::OnceClosure callback,
              ErrorCallback error_callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(error_callback).Run(kInvalidPathError, "");
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kRemoveServiceFunction);
    helper->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                            std::move(error_callback));
  }

  void CompleteCellularActivation(const dbus::ObjectPath& service_path,
                                  base::OnceClosure callback,
                                  ErrorCallback error_callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(error_callback).Run(kInvalidPathError, "");
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kCompleteCellularActivationFunction);
    dbus::MessageWriter writer(&method_call);
    helper->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                            std::move(error_callback));
  }

  void GetLoadableProfileEntries(
      const dbus::ObjectPath& service_path,
      chromeos::DBusMethodCallback<base::Value::Dict> callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(callback).Run(base::Value::Dict());
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kGetLoadableProfileEntriesFunction);
    auto split_callback = base::SplitOnceCallback(std::move(callback));
    helper->CallDictValueMethodWithErrorCallback(
        &method_call,
        AdaptCallbackWithoutStatus(std::move(split_callback.first)),
        base::BindOnce(&OnGetDictionaryError, "GetLoadableProfileEntries",
                       service_path, std::move(split_callback.second)));
  }

  void GetWiFiPassphrase(const dbus::ObjectPath& service_path,
                         StringCallback callback,
                         ErrorCallback error_callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(error_callback).Run(kInvalidPathError, "");
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kGetWiFiPassphraseFunction);
    helper->CallStringMethodWithErrorCallback(&method_call, std::move(callback),
                                              std::move(error_callback));
  }

  void GetEapPassphrase(const dbus::ObjectPath& service_path,
                        StringCallback callback,
                        ErrorCallback error_callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(error_callback).Run(kInvalidPathError, "");
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kGetEapPassphraseFunction);
    helper->CallStringMethodWithErrorCallback(&method_call, std::move(callback),
                                              std::move(error_callback));
  }

  void RequestPortalDetection(
      const dbus::ObjectPath& service_path,
      chromeos::VoidDBusMethodCallback callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(callback).Run(false);
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kRequestPortalDetectionFunction);
    helper->CallVoidMethod(&method_call, std::move(callback));
  }

  void RequestTrafficCounters(
      const dbus::ObjectPath& service_path,
      chromeos::DBusMethodCallback<base::Value> callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(callback).Run(base::Value());
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kRequestTrafficCountersFunction);
    helper->CallValueMethod(&method_call, std::move(callback));
  }

  void ResetTrafficCounters(const dbus::ObjectPath& service_path,
                            base::OnceClosure callback,
                            ErrorCallback error_callback) override {
    auto* helper = GetHelper(service_path);
    if (!helper) {
      std::move(error_callback).Run(kInvalidPathError, "");
      return;
    }
    dbus::MethodCall method_call(shill::kFlimflamServiceInterface,
                                 shill::kResetTrafficCountersFunction);
    helper->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                            std::move(error_callback));
  }

  ShillServiceClient::TestInterface* GetTestInterface() override {
    return nullptr;
  }

 private:
  using HelperMap =
      std::map<std::string, raw_ptr<ShillClientHelper, CtnExperimental>>;

  // Returns the corresponding ShillClientHelper for the profile.
  ShillClientHelper* GetHelper(const dbus::ObjectPath& service_path) {
    static const std::string kServicePrefix("/service/");
    if (service_path.value().compare(0, kServicePrefix.size(),
                                     kServicePrefix) != 0) {
      LOG(ERROR) << "Invalid service path: " << service_path.value();
      return nullptr;
    }

    HelperMap::iterator it = helpers_.find(service_path.value());
    if (it != helpers_.end()) {
      return it->second;
    }

    // There is no helper for the profile, create it.
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(shill::kFlimflamServiceName, service_path);
    ShillClientHelper* helper = new ShillClientHelper(object_proxy);
    helper->SetReleasedCallback(
        base::BindOnce(&ShillServiceClientImpl::NotifyReleased,
                       weak_ptr_factory_.GetWeakPtr()));
    helper->MonitorPropertyChanged(shill::kFlimflamServiceInterface);
    helpers_.insert(HelperMap::value_type(service_path.value(), helper));
    return helper;
  }

  void NotifyReleased(ShillClientHelper* helper) {
    // New Shill Service DBus objects are created relatively frequently, so
    // remove them when they become inactive (no observers and no active method
    // calls).
    dbus::ObjectPath object_path = helper->object_proxy()->object_path();
    // Make sure we don't release the proxy used by ShillManagerClient ("/").
    // This shouldn't ever happen, but might if a bug in the code requests
    // a service with path "/", or a bug in Shill passes "/" as a service path.
    // Either way this would cause an invalid memory access in
    // ShillManagerClient, see crbug.com/324849.
    if (object_path == dbus::ObjectPath(shill::kFlimflamServicePath)) {
      NET_LOG(ERROR) << "ShillServiceClient service has invalid path: "
                     << shill::kFlimflamServicePath;
      return;
    }
    bus_->RemoveObjectProxy(shill::kFlimflamServiceName, object_path,
                            base::DoNothing());
    helpers_.erase(object_path.value());
    delete helper;
  }

  static base::OnceCallback<void(base::Value::Dict result)>
  AdaptCallbackWithoutStatus(
      chromeos::DBusMethodCallback<base::Value::Dict> callback) {
    return base::BindOnce(
        [](chromeos::DBusMethodCallback<base::Value::Dict> callback,
           base::Value::Dict result) {
          std::move(callback).Run(std::move(result));
        },
        std::move(callback));
  }

  raw_ptr<dbus::Bus> bus_;
  HelperMap helpers_;
  base::WeakPtrFactory<ShillServiceClientImpl> weak_ptr_factory_{this};
};

}  // namespace

ShillServiceClient::ShillServiceClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

ShillServiceClient::~ShillServiceClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ShillServiceClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  new ShillServiceClientImpl(bus);
}

// static
void ShillServiceClient::InitializeFake() {
  new FakeShillServiceClient();
}

// static
void ShillServiceClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
ShillServiceClient* ShillServiceClient::Get() {
  return g_instance;
}

}  // namespace ash
