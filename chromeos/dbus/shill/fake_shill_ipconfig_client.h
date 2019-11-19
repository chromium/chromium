// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_FAKE_SHILL_IPCONFIG_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_FAKE_SHILL_IPCONFIG_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/shill/shill_ipconfig_client.h"

namespace chromeos {

// A fake implementation of ShillIPConfigClient.
class COMPONENT_EXPORT(SHILL_CLIENT) FakeShillIPConfigClient
    : public ShillIPConfigClient,
      public ShillIPConfigClient::TestInterface {
 public:
  FakeShillIPConfigClient();
  ~FakeShillIPConfigClient() override;

  // ShillIPConfigClient overrides
  void AddPropertyChangedObserver(
      const dbus::ObjectPath& ipconfig_path,
      ShillPropertyChangedObserver* observer) override;
  void RemovePropertyChangedObserver(
      const dbus::ObjectPath& ipconfig_path,
      ShillPropertyChangedObserver* observer) override;
  void GetProperties(const dbus::ObjectPath& ipconfig_path,
                     const DictionaryValueCallback& callback) override;
  void SetProperty(const dbus::ObjectPath& ipconfig_path,
                   const std::string& name,
                   const base::Value& value,
                   VoidDBusMethodCallback callback) override;
  void ClearProperty(const dbus::ObjectPath& ipconfig_path,
                     const std::string& name,
                     VoidDBusMethodCallback callback) override;
  void Remove(const dbus::ObjectPath& ipconfig_path,
              VoidDBusMethodCallback callback) override;
  ShillIPConfigClient::TestInterface* GetTestInterface() override;

  // ShillIPConfigClient::TestInterface overrides.
  void AddIPConfig(const std::string& ip_config_path,
                   const base::DictionaryValue& properties) override;

 private:
  // Runs callback with |values|.
  void PassProperties(const base::DictionaryValue* values,
                      const DictionaryValueCallback& callback) const;

  // Dictionary of <ipconfig_path, property dictionaries>
  base::DictionaryValue ipconfigs_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeShillIPConfigClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeShillIPConfigClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_FAKE_SHILL_IPCONFIG_CLIENT_H_
