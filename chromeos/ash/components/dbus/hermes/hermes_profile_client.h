// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_PROFILE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_PROFILE_CLIENT_H_

#include <string>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "dbus/property.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace dbus {
class Bus;
class ObjectPath;
class ObjectProxy;
}  // namespace dbus

namespace ash {

// HermesProfileClient is used to talk to Hermes profile objects.
class COMPONENT_EXPORT(HERMES_CLIENT) HermesProfileClient {
 public:
  class TestInterface {
   public:
    enum class EnableProfileBehavior {
      kNotConnectable,
      kConnectableButNotConnected,
      kConnectableAndConnected
    };

    // Clears the Profile properties for the given path.
    virtual void ClearProfile(const dbus::ObjectPath& carrier_profile_path) = 0;
    // Sets service state to connected after eSIM profiles are enabled.
    virtual void SetEnableProfileBehavior(
        EnableProfileBehavior enable_profile_behavior) = 0;

    // Sets the return for the next call to
    // HermesEuiccClient::EnableCarrierProfile(). The implementation of this
    // method should only accept error statuses since clients expect additional
    // steps to have been taken when successful.
    virtual void SetNextEnableCarrierProfileResult(
        HermesResponseStatus status) = 0;
  };

  // Hermes profile properties.
  class Properties : public dbus::PropertySet {
   public:
    Properties(dbus::ObjectProxy* object_proxy,
               const PropertyChangedCallback& callback);
    ~Properties() override;

    dbus::Property<std::string>& iccid() { return iccid_; }
    dbus::Property<std::string>& service_provider() {
      return service_provider_;
    }
    dbus::Property<std::string>& mcc_mnc() { return mcc_mnc_; }
    dbus::Property<std::string>& activation_code() { return activation_code_; }
    dbus::Property<std::string>& name() { return name_; }
    dbus::Property<std::string>& nick_name() { return nick_name_; }
    dbus::Property<hermes::profile::State>& state() { return state_; }
    dbus::Property<hermes::profile::ProfileClass>& profile_class() {
      return profile_class_;
    }

   private:
    dbus::Property<std::string> iccid_;
    dbus::Property<std::string> service_provider_;
    dbus::Property<std::string> mcc_mnc_;
    dbus::Property<std::string> activation_code_;
    dbus::Property<std::string> name_;
    dbus::Property<std::string> nick_name_;
    dbus::Property<hermes::profile::State> state_;
    dbus::Property<hermes::profile::ProfileClass> profile_class_;
  };

  // Interface for observing changes to profile objects.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when a carrier profile property changes.
    virtual void OnCarrierProfilePropertyChanged(
        const dbus::ObjectPath& object_path,
        const std::string& property_name) = 0;
  };

  // Adds an observer for changes to Hermes profile objects.
  virtual void AddObserver(Observer* observer);

  // Removes an observer for Hermes profile objects.
  virtual void RemoveObserver(Observer* observer);

  // Enables eSIM carrier profile with given |carrier_profile_path|. |callback|
  // will receive status code indicating response status.
  virtual void EnableCarrierProfile(
      const dbus::ObjectPath& carrier_profile_path,
      HermesResponseCallback callback) = 0;

  // Disables eSIM carrier profile with given |carrier_profile_path|. |callback|
  // will receive status code indicating response status.
  virtual void DisableCarrierProfile(
      const dbus::ObjectPath& carrier_profile_path,
      HermesResponseCallback callback) = 0;

  // Rename the profile's nick name to |new_name| with given
  // |carrier_profile_path|. |callback| will receive status code indicating
  // response status.
  virtual void RenameProfile(const dbus::ObjectPath& carrier_profile_path,
                             const std::string& new_name,
                             HermesResponseCallback callback) = 0;

  // Returns properties for eSIM carrier profile with given
  // |carrier_profile_path|.
  virtual Properties* GetProperties(
      const dbus::ObjectPath& carrier_profile_path) = 0;

  // Returns an instance of Hermes Profile Test interface.
  virtual TestInterface* GetTestInterface() = 0;

  // Creates and initializes the global instance.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a global fake instance.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance.
  static HermesProfileClient* Get();

 protected:
  HermesProfileClient();
  virtual ~HermesProfileClient();

  const base::ObserverList<HermesProfileClient::Observer>::Unchecked&
  observers() {
    return observers_;
  }

 private:
  base::ObserverList<HermesProfileClient::Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_PROFILE_CLIENT_H_
