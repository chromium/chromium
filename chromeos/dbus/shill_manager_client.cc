// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_manager_client.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "chromeos/dbus/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "net/base/ip_endpoint.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// The ShillManagerClient implementation.
class ShillManagerClientImpl : public ShillManagerClient {
 public:
  ShillManagerClientImpl() : proxy_(NULL) {}

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

  void GetProperties(const DictionaryValueCallback& callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kGetPropertiesFunction);
    helper_->CallDictionaryValueMethod(&method_call, callback);
  }

  void GetNetworksForGeolocation(
      const DictionaryValueCallback& callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kGetNetworksForGeolocation);
    helper_->CallDictionaryValueMethod(&method_call, callback);
  }

  void SetProperty(const std::string& name,
                   const base::Value& value,
                   const base::Closure& callback,
                   const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kSetPropertyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(name);
    ShillClientHelper::AppendValueDataAsVariant(&writer, value);
    helper_->CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  void RequestScan(const std::string& type,
                   const base::Closure& callback,
                   const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kRequestScanFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_->CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  void EnableTechnology(const std::string& type,
                        const base::Closure& callback,
                        const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kEnableTechnologyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_->CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  void SetNetworkThrottlingStatus(
      const NetworkThrottlingStatus& status,
      const base::Closure& callback,
      const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kSetNetworkThrottlingFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendBool(status.enabled);
    writer.AppendUint32(status.upload_rate_kbits);
    writer.AppendUint32(status.download_rate_kbits);
    helper_->CallVoidMethodWithErrorCallback(&method_call, callback,
                                             error_callback);
  }

  void DisableTechnology(const std::string& type,
                         const base::Closure& callback,
                         const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kDisableTechnologyFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(type);
    helper_->CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  void ConfigureService(const base::DictionaryValue& properties,
                        const ObjectPathCallback& callback,
                        const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kConfigureServiceFunction);
    dbus::MessageWriter writer(&method_call);
    ShillClientHelper::AppendServicePropertiesDictionary(&writer, properties);
    helper_->CallObjectPathMethodWithErrorCallback(&method_call,
                                                  callback,
                                                  error_callback);
  }

  void ConfigureServiceForProfile(
      const dbus::ObjectPath& profile_path,
      const base::DictionaryValue& properties,
      const ObjectPathCallback& callback,
      const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kConfigureServiceForProfileFunction);
    dbus::MessageWriter writer(&method_call);
    writer.AppendObjectPath(dbus::ObjectPath(profile_path));
    ShillClientHelper::AppendServicePropertiesDictionary(&writer, properties);
    helper_->CallObjectPathMethodWithErrorCallback(&method_call,
                                                  callback,
                                                  error_callback);
  }

  void GetService(const base::DictionaryValue& properties,
                  const ObjectPathCallback& callback,
                  const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kGetServiceFunction);
    dbus::MessageWriter writer(&method_call);
    ShillClientHelper::AppendServicePropertiesDictionary(&writer, properties);
    helper_->CallObjectPathMethodWithErrorCallback(&method_call,
                                                  callback,
                                                  error_callback);
  }

  void ConnectToBestServices(const base::Closure& callback,
                             const ErrorCallback& error_callback) override {
    dbus::MethodCall method_call(shill::kFlimflamManagerInterface,
                                 shill::kConnectToBestServicesFunction);
    helper_->CallVoidMethodWithErrorCallback(&method_call,
                                            callback,
                                            error_callback);
  }

  TestInterface* GetTestInterface() override { return NULL; }

 protected:
  void Init(dbus::Bus* bus) override {
    proxy_ = bus->GetObjectProxy(shill::kFlimflamServiceName,
                                 dbus::ObjectPath(shill::kFlimflamServicePath));
    helper_.reset(new ShillClientHelper(proxy_));
    helper_->MonitorPropertyChanged(shill::kFlimflamManagerInterface);
  }

 private:
  dbus::ObjectProxy* proxy_;
  std::unique_ptr<ShillClientHelper> helper_;

  DISALLOW_COPY_AND_ASSIGN(ShillManagerClientImpl);
};

}  // namespace

ShillManagerClient::ShillManagerClient() = default;

ShillManagerClient::~ShillManagerClient() = default;

// static
ShillManagerClient* ShillManagerClient::Create() {
  return new ShillManagerClientImpl();
}

}  // namespace chromeos
