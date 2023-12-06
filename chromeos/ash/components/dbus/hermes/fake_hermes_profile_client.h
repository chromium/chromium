// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_FAKE_HERMES_PROFILE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_FAKE_HERMES_PROFILE_CLIENT_H_

#include <map>
#include <memory>
#include <optional>

#include "base/component_export.h"
#include "chromeos/ash/components/dbus/hermes/hermes_profile_client.h"
#include "dbus/object_path.h"
#include "dbus/property.h"

namespace ash {

// Fake implementation for HermesProfileClient.
class COMPONENT_EXPORT(HERMES_CLIENT) FakeHermesProfileClient
    : public HermesProfileClient,
      public HermesProfileClient::TestInterface {
 public:
  class Properties : public HermesProfileClient::Properties {
   public:
    explicit Properties(const PropertyChangedCallback& callback);
    ~Properties() override;

    // dbus::PropertySet:
    void Get(dbus::PropertyBase* property,
             dbus::PropertySet::GetCallback callback) override;
    void GetAll() override;
    void Set(dbus::PropertyBase* property,
             dbus::PropertySet::SetCallback callback) override;
  };

  FakeHermesProfileClient();
  FakeHermesProfileClient(const FakeHermesProfileClient&) = delete;
  FakeHermesProfileClient& operator=(const FakeHermesProfileClient&) = delete;
  ~FakeHermesProfileClient() override;

  // HermesProfileClient::TestInterface:
  void ClearProfile(const dbus::ObjectPath& carrier_profile_path) override;
  void SetEnableProfileBehavior(
      EnableProfileBehavior enable_profile_behavior) override;
  void SetNextEnableCarrierProfileResult(HermesResponseStatus status) override;

  // HermesProfileClient:
  void EnableCarrierProfile(const dbus::ObjectPath& object_path,
                            HermesResponseCallback callback) override;
  void DisableCarrierProfile(const dbus::ObjectPath& object_path,
                             HermesResponseCallback callback) override;
  void RenameProfile(const dbus::ObjectPath& object_path,
                     const std::string& new_name,
                     HermesResponseCallback callback) override;
  HermesProfileClient::Properties* GetProperties(
      const dbus::ObjectPath& object_path) override;
  HermesProfileClient::TestInterface* GetTestInterface() override;

 private:
  void UpdateCellularDevice(HermesProfileClient::Properties* properties);
  void UpdateCellularServices(const std::string& iccid, bool connectable);
  void CallNotifyPropertyChanged(const dbus::ObjectPath& object_path,
                                 const std::string& property_name);
  void NotifyPropertyChanged(const dbus::ObjectPath& object_path,
                             const std::string& property_name);

  EnableProfileBehavior enable_profile_behavior_ =
      EnableProfileBehavior::kConnectableButNotConnected;

  // Maps fake profile properties to their object paths.
  using PropertiesMap =
      std::map<const dbus::ObjectPath, std::unique_ptr<Properties>>;
  PropertiesMap properties_map_;

  // When set, this will be returned as the result of the next attempt to enable
  // a carrier profile.
  std::optional<HermesResponseStatus> next_enable_carrier_profile_result_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_FAKE_HERMES_PROFILE_CLIENT_H_
