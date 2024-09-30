// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/shill_device_client.h"

#include <map>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "net/base/ip_endpoint.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

ShillDeviceClient* g_instance = nullptr;

// The ShillDeviceClient implementation.
class ShillDeviceClientImpl : public ShillDeviceClient {
 public:
  explicit ShillDeviceClientImpl(dbus::Bus* bus) : bus_(bus) {}

  ShillDeviceClientImpl(const ShillDeviceClientImpl&) = delete;
  ShillDeviceClientImpl& operator=(const ShillDeviceClientImpl&) = delete;

  ~ShillDeviceClientImpl() override {
    for (HelperMap::iterator iter = helpers_.begin(); iter != helpers_.end();
         ++iter) {
      // This *should* never happen, yet we're getting crash reports that
      // seem to imply that it does happen sometimes.  Adding CHECKs here
      // so we can determine more accurately where the problem lies.
      // See: http://crbug.com/170541
      CHECK(iter->second) << "null Helper found in helper list.";
      delete iter->second;
    }
    helpers_.clear();
  }

  ///////////////////////////////////////
  // ShillDeviceClient overrides.
  void AddPropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) override {
    GetHelper(device_path)->AddPropertyChangedObserver(observer);
  }

  void RemovePropertyChangedObserver(
      const dbus::ObjectPath& device_path,
      ShillPropertyChangedObserver* observer) override {
    GetHelper(device_path)->RemovePropertyChangedObserver(observer);
  }

  void GetProperties(
      const dbus::ObjectPath& device_path,
      chromeos::DBusMethodCallback<base::Value::Dict> callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kGetPropertiesFunction);
    GetHelper(device_path)
        ->CallDictValueMethod(&method_call, std::move(callback));
  }

  void SetProperty(const dbus::ObjectPath& device_path,
                   const std::string& name,
                   const base::Value& value,
                   base::OnceClosure callback,
                   ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    ShillClientHelper::AppendValueDataAsVariant(&writer, name, value);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void ClearProperty(const dbus::ObjectPath& device_path,
                     const std::string& name,
                     chromeos::VoidDBusMethodCallback callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kClearPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    GetHelper(device_path)->CallVoidMethod(&method_call, std::move(callback));
  }

  void RequirePin(const dbus::ObjectPath& device_path,
                  const std::string& pin,
                  bool require,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kRequirePinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(pin);
    writer.AppendBool(require);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void EnterPin(const dbus::ObjectPath& device_path,
                const std::string& pin,
                base::OnceClosure callback,
                ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kEnterPinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(pin);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void UnblockPin(const dbus::ObjectPath& device_path,
                  const std::string& puk,
                  const std::string& pin,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kUnblockPinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(puk);
    writer.AppendString(pin);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void ChangePin(const dbus::ObjectPath& device_path,
                 const std::string& old_pin,
                 const std::string& new_pin,
                 base::OnceClosure callback,
                 ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kChangePinFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(old_pin);
    writer.AppendString(new_pin);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void Register(const dbus::ObjectPath& device_path,
                const std::string& network_id,
                base::OnceClosure callback,
                ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kRegisterFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(network_id);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void Reset(const dbus::ObjectPath& device_path,
             base::OnceClosure callback,
             ErrorCallback error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamDeviceInterface,
                                 shill::kResetFunction);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  void SetUsbEthernetMacAddressSource(const dbus::ObjectPath& device_path,
                                      const std::string& source,
                                      base::OnceClosure callback,
                                      ErrorCallback error_callback) override {
    dbus::MethodCall method_call(
        shill::kFlimflamDeviceInterface,
        shill::kSetUsbEthernetMacAddressSourceFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(source);
    GetHelper(device_path)
        ->CallVoidMethodWithErrorCallback(&method_call, std::move(callback),
                                          std::move(error_callback));
  }

  TestInterface* GetTestInterface() override { return nullptr; }

 private:
  typedef std::map<std::string, raw_ptr<ShillClientHelper, CtnExperimental>>
      HelperMap;

  // Returns the corresponding ShillClientHelper for the profile.
  ShillClientHelper* GetHelper(const dbus::ObjectPath& device_path) {
    HelperMap::iterator it = helpers_.find(device_path.value());
    if (it != helpers_.end()) {
      CHECK(it->second) << "Found a null helper in the list.";
      return it->second;
    }

    // There is no helper for the profile, create it.
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(shill::kFlimflamServiceName, device_path);
    ShillClientHelper* helper = new ShillClientHelper(object_proxy);
    CHECK(helper) << "Unable to create Shill client helper.";
    helper->MonitorPropertyChanged(shill::kFlimflamDeviceInterface);
    helpers_.insert(HelperMap::value_type(device_path.value(), helper));
    return helper;
  }

  raw_ptr<dbus::Bus> bus_;
  HelperMap helpers_;
};

}  // namespace

ShillDeviceClient::ShillDeviceClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

ShillDeviceClient::~ShillDeviceClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ShillDeviceClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  new ShillDeviceClientImpl(bus);
}

// static
void ShillDeviceClient::InitializeFake() {
  new FakeShillDeviceClient();
}

// static
void ShillDeviceClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
ShillDeviceClient* ShillDeviceClient::Get() {
  return g_instance;
}

}  // namespace ash
