// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/dbus/shill/fake_shill_ipconfig_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "chromeos/ash/components/dbus/shill/shill_property_changed_observer.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/object_proxy.h"
#include "dbus/values_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

FakeShillIPConfigClient::FakeShillIPConfigClient() = default;

FakeShillIPConfigClient::~FakeShillIPConfigClient() = default;

void FakeShillIPConfigClient::AddPropertyChangedObserver(
    const dbus::ObjectPath& ipconfig_path,
    ShillPropertyChangedObserver* observer) {}

void FakeShillIPConfigClient::RemovePropertyChangedObserver(
    const dbus::ObjectPath& ipconfig_path,
    ShillPropertyChangedObserver* observer) {}

void FakeShillIPConfigClient::GetProperties(
    const dbus::ObjectPath& ipconfig_path,
    chromeos::DBusMethodCallback<base::Value::Dict> callback) {
  const base::Value::Dict* dict = ipconfigs_.EnsureDict(ipconfig_path.value());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), dict->Clone()));
}

void FakeShillIPConfigClient::SetProperty(
    const dbus::ObjectPath& ipconfig_path,
    const std::string& name,
    const base::Value& value,
    chromeos::VoidDBusMethodCallback callback) {
  base::Value::Dict* dict = ipconfigs_.EnsureDict(ipconfig_path.value());

  // Update existing ip config stub object's properties.
  dict->Set(name, value.Clone());
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeShillIPConfigClient::ClearProperty(
    const dbus::ObjectPath& ipconfig_path,
    const std::string& name,
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

void FakeShillIPConfigClient::Remove(
    const dbus::ObjectPath& ipconfig_path,
    chromeos::VoidDBusMethodCallback callback) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

ShillIPConfigClient::TestInterface*
FakeShillIPConfigClient::GetTestInterface() {
  return this;
}

// ShillIPConfigClient::TestInterface overrides

void FakeShillIPConfigClient::AddIPConfig(const std::string& ip_config_path,
                                          base::Value::Dict properties) {
  ipconfigs_.Set(ip_config_path, std::move(properties));
}

}  // namespace ash
