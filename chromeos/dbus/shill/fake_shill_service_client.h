// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_SHILL_FAKE_SHILL_SERVICE_CLIENT_H_
#define CHROMEOS_DBUS_SHILL_FAKE_SHILL_SERVICE_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/shill/shill_service_client.h"

namespace chromeos {

// A fake implementation of ShillServiceClient. This works in close coordination
// with FakeShillManagerClient and is not intended to be used independently.
class COMPONENT_EXPORT(SHILL_CLIENT) FakeShillServiceClient
    : public ShillServiceClient,
      public ShillServiceClient::TestInterface {
 public:
  FakeShillServiceClient();
  ~FakeShillServiceClient() override;

  // ShillServiceClient overrides
  void AddPropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) override;
  void RemovePropertyChangedObserver(
      const dbus::ObjectPath& service_path,
      ShillPropertyChangedObserver* observer) override;
  void GetProperties(const dbus::ObjectPath& service_path,
                     const DictionaryValueCallback& callback) override;
  void SetProperty(const dbus::ObjectPath& service_path,
                   const std::string& name,
                   const base::Value& value,
                   const base::Closure& callback,
                   const ErrorCallback& error_callback) override;
  void SetProperties(const dbus::ObjectPath& service_path,
                     const base::DictionaryValue& properties,
                     const base::Closure& callback,
                     const ErrorCallback& error_callback) override;
  void ClearProperty(const dbus::ObjectPath& service_path,
                     const std::string& name,
                     const base::Closure& callback,
                     const ErrorCallback& error_callback) override;
  void ClearProperties(const dbus::ObjectPath& service_path,
                       const std::vector<std::string>& names,
                       const ListValueCallback& callback,
                       const ErrorCallback& error_callback) override;
  void Connect(const dbus::ObjectPath& service_path,
               const base::Closure& callback,
               const ErrorCallback& error_callback) override;
  void Disconnect(const dbus::ObjectPath& service_path,
                  const base::Closure& callback,
                  const ErrorCallback& error_callback) override;
  void Remove(const dbus::ObjectPath& service_path,
              const base::Closure& callback,
              const ErrorCallback& error_callback) override;
  void ActivateCellularModem(const dbus::ObjectPath& service_path,
                             const std::string& carrier,
                             const base::Closure& callback,
                             const ErrorCallback& error_callback) override;
  void CompleteCellularActivation(const dbus::ObjectPath& service_path,
                                  const base::Closure& callback,
                                  const ErrorCallback& error_callback) override;
  void GetLoadableProfileEntries(
      const dbus::ObjectPath& service_path,
      const DictionaryValueCallback& callback) override;
  ShillServiceClient::TestInterface* GetTestInterface() override;

  // ShillServiceClient::TestInterface overrides.
  void AddService(const std::string& service_path,
                  const std::string& guid,
                  const std::string& name,
                  const std::string& type,
                  const std::string& state,
                  bool visible) override;
  void AddServiceWithIPConfig(const std::string& service_path,
                              const std::string& guid,
                              const std::string& name,
                              const std::string& type,
                              const std::string& state,
                              const std::string& ipconfig_path,
                              bool visible) override;
  base::DictionaryValue* SetServiceProperties(const std::string& service_path,
                                              const std::string& guid,
                                              const std::string& name,
                                              const std::string& type,
                                              const std::string& state,
                                              bool visible) override;
  void RemoveService(const std::string& service_path) override;
  bool SetServiceProperty(const std::string& service_path,
                          const std::string& property,
                          const base::Value& value) override;
  const base::DictionaryValue* GetServiceProperties(
      const std::string& service_path) const override;
  bool ClearConfiguredServiceProperties(
      const std::string& service_path) override;
  std::string FindServiceMatchingGUID(const std::string& guid) override;
  std::string FindSimilarService(
      const base::Value& template_service_properties) override;
  void ClearServices() override;
  void SetConnectBehavior(const std::string& service_path,
                          const base::Closure& behavior) override;
  void SetHoldBackServicePropertyUpdates(bool hold_back) override;

 private:
  typedef base::ObserverList<ShillPropertyChangedObserver>::Unchecked
      PropertyObserverList;

  void NotifyObserversPropertyChanged(const dbus::ObjectPath& service_path,
                                      const std::string& property);
  base::DictionaryValue* GetModifiableServiceProperties(
      const std::string& service_path,
      bool create_if_missing);
  PropertyObserverList& GetObserverList(const dbus::ObjectPath& device_path);
  void SetOtherServicesOffline(const std::string& service_path);
  void SetCellularActivated(const dbus::ObjectPath& service_path,
                            const ErrorCallback& error_callback);
  void ContinueConnect(const std::string& service_path);

  base::DictionaryValue stub_services_;

  // Per network service, stores a closure that is executed on each connection
  // attempt. The callback can for example modify the services properties in
  // order to simulate a connection failure.
  std::map<std::string, base::Closure> connect_behavior_;

  // Observer list for each service.
  std::map<dbus::ObjectPath, std::unique_ptr<PropertyObserverList>>
      observer_list_;

  // If this is true, the FakeShillServiceClient is recording service property
  // updates and will only send them when SetHoldBackServicePropertyUpdates is
  // called with false again.
  bool hold_back_service_property_updates_ = false;

  // Property updates that were held back while
  // |hold_back_service_property_updates_| was true.
  std::vector<base::OnceClosure> recorded_property_updates_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<FakeShillServiceClient> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeShillServiceClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_SHILL_FAKE_SHILL_SERVICE_CLIENT_H_
