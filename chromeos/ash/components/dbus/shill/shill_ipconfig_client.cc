// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/shill_ipconfig_client.h"

#include <map>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_ipconfig_client.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

ShillIPConfigClient* g_instance = nullptr;

// The ShillIPConfigClient implementation.
class ShillIPConfigClientImpl : public ShillIPConfigClient {
 public:
  explicit ShillIPConfigClientImpl(dbus::Bus* bus) : bus_(bus) {}

  ShillIPConfigClientImpl(const ShillIPConfigClientImpl&) = delete;
  ShillIPConfigClientImpl& operator=(const ShillIPConfigClientImpl&) = delete;

  ~ShillIPConfigClientImpl() override = default;

  ////////////////////////////////////
  // ShillIPConfigClient overrides.
  void AddPropertyChangedObserver(
      const dbus::ObjectPath& ipconfig_path,
      ShillPropertyChangedObserver* observer) override {
    GetHelper(ipconfig_path)->AddPropertyChangedObserver(observer);
  }

  void RemovePropertyChangedObserver(
      const dbus::ObjectPath& ipconfig_path,
      ShillPropertyChangedObserver* observer) override {
    GetHelper(ipconfig_path)->RemovePropertyChangedObserver(observer);
  }
  void GetProperties(
      const dbus::ObjectPath& ipconfig_path,
      chromeos::DBusMethodCallback<base::Value::Dict> callback) override;
  void SetProperty(const dbus::ObjectPath& ipconfig_path,
                   const std::string& name,
                   const base::Value& value,
                   chromeos::VoidDBusMethodCallback callback) override;
  void ClearProperty(const dbus::ObjectPath& ipconfig_path,
                     const std::string& name,
                     chromeos::VoidDBusMethodCallback callback) override;
  void Remove(const dbus::ObjectPath& ipconfig_path,
              chromeos::VoidDBusMethodCallback callback) override;
  ShillIPConfigClient::TestInterface* GetTestInterface() override;

 private:
  using HelperMap = std::map<std::string, std::unique_ptr<ShillClientHelper>>;

  // Returns the corresponding ShillClientHelper for the profile.
  ShillClientHelper* GetHelper(const dbus::ObjectPath& ipconfig_path) {
    HelperMap::const_iterator it = helpers_.find(ipconfig_path.value());
    if (it != helpers_.end())
      return it->second.get();

    // There is no helper for the profile, create it.
    dbus::ObjectProxy* object_proxy =
        bus_->GetObjectProxy(shill::kFlimflamServiceName, ipconfig_path);
    std::unique_ptr<ShillClientHelper> helper(
        new ShillClientHelper(object_proxy));
    helper->MonitorPropertyChanged(shill::kFlimflamIPConfigInterface);
    ShillClientHelper* helper_ptr = helper.get();
    helpers_[ipconfig_path.value()] = std::move(helper);
    return helper_ptr;
  }

  raw_ptr<dbus::Bus> bus_;
  HelperMap helpers_;
};

void ShillIPConfigClientImpl::GetProperties(
    const dbus::ObjectPath& ipconfig_path,
    chromeos::DBusMethodCallback<base::Value::Dict> callback) {
  dbus::MethodCall method_call(shill::kFlimflamIPConfigInterface,
                               shill::kGetPropertiesFunction);
  GetHelper(ipconfig_path)
      ->CallDictValueMethod(&method_call, std::move(callback));
}

void ShillIPConfigClientImpl::SetProperty(
    const dbus::ObjectPath& ipconfig_path,
    const std::string& name,
    const base::Value& value,
    chromeos::VoidDBusMethodCallback callback) {
  dbus::MethodCall method_call(shill::kFlimflamIPConfigInterface,
                               shill::kSetPropertyFunction);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(name);
  // IPConfig supports writing basic type and string array properties.
  switch (value.type()) {
    case base::Value::Type::LIST: {
      dbus::MessageWriter variant_writer(nullptr);
      writer.OpenVariant("as", &variant_writer);
      dbus::MessageWriter array_writer(nullptr);
      variant_writer.OpenArray("s", &array_writer);
      for (const auto& entry : value.GetList()) {
        DLOG_IF(ERROR, !entry.is_string())
            << "Unexpected type " << entry.type();
        array_writer.AppendString(entry.is_string() ? entry.GetString()
                                                    : std::string());
      }
      variant_writer.CloseContainer(&array_writer);
      writer.CloseContainer(&variant_writer);
      break;
    }
    case base::Value::Type::BOOLEAN:
    case base::Value::Type::INTEGER:
    case base::Value::Type::DOUBLE:
    case base::Value::Type::STRING:
      dbus::AppendBasicTypeValueDataAsVariant(&writer, value);
      break;
    default:
      DLOG(ERROR) << "Unexpected type " << value.type();
  }
  GetHelper(ipconfig_path)->CallVoidMethod(&method_call, std::move(callback));
}

void ShillIPConfigClientImpl::ClearProperty(
    const dbus::ObjectPath& ipconfig_path,
    const std::string& name,
    chromeos::VoidDBusMethodCallback callback) {
  dbus::MethodCall method_call(shill::kFlimflamIPConfigInterface,
                               shill::kClearPropertyFunction);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(name);
  GetHelper(ipconfig_path)->CallVoidMethod(&method_call, std::move(callback));
}

void ShillIPConfigClientImpl::Remove(
    const dbus::ObjectPath& ipconfig_path,
    chromeos::VoidDBusMethodCallback callback) {
  dbus::MethodCall method_call(shill::kFlimflamIPConfigInterface,
                               shill::kRemoveConfigFunction);
  GetHelper(ipconfig_path)->CallVoidMethod(&method_call, std::move(callback));
}

ShillIPConfigClient::TestInterface*
ShillIPConfigClientImpl::GetTestInterface() {
  return nullptr;
}

}  // namespace

ShillIPConfigClient::ShillIPConfigClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

ShillIPConfigClient::~ShillIPConfigClient() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
void ShillIPConfigClient::Initialize(dbus::Bus* bus) {
  DCHECK(bus);
  new ShillIPConfigClientImpl(bus);
}

// static
void ShillIPConfigClient::InitializeFake() {
  new FakeShillIPConfigClient();
}

// static
void ShillIPConfigClient::Shutdown() {
  DCHECK(g_instance);
  delete g_instance;
}

// static
ShillIPConfigClient* ShillIPConfigClient::Get() {
  return g_instance;
}

}  // namespace ash
