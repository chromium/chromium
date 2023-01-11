// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_EUICC_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_EUICC_CLIENT_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/hermes/hermes_response_status.h"
#include "chromeos/dbus/common/dbus_method_call_status.h"
#include "dbus/property.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace ash {

// HermesEuiccClient is used to talk to the Hermes Euicc objects.
class COMPONENT_EXPORT(HERMES_CLIENT) HermesEuiccClient {
 public:
  // Callback for profile installation methods. Callback returns status code
  // and the object path for the profile that was just successfully installed.
  using InstallCarrierProfileCallback =
      base::OnceCallback<void(HermesResponseStatus status,
                              const dbus::ObjectPath* carrier_profile_path)>;

  class TestInterface {
   public:
    enum class AddCarrierProfileBehavior {
      // Adds a profile which can be immediately retrieved from a EUICC by
      // examining its installed profiles. Also creates a Shill service
      // representing the profile.
      kAddProfileWithService,

      // Adds a profile which is not yet present of the EUICC's properties but
      // will be available after a subsequent call to
      // RefreshInstalledProfiles(). Also creates a Shill service representing
      // the profile.
      kAddDelayedProfileWithService,

      // Adds a profile which can be immediately retrieved from a EUICC by
      // examining its installed profiles. Does not create an accompanying Shill
      // service representing the profile, which simulates a "stub" cellular
      // service.
      kAddProfileWithoutService,

      // Adds a profile which is not yet present of the EUICC's properties but
      // will be available after a subsequent call to
      // RefreshInstalledProfiles(). Does not create an accompanying Shill
      // service representing the profile, which simulates a "stub" cellular
      // service.
      kAddDelayedProfileWithoutService,
    };

    // Clears a given Euicc and associated profiles.
    virtual void ClearEuicc(const dbus::ObjectPath& euicc_path) = 0;

    // Resets the test interface for a new fake pending profile
    // will be added on subsequent  call to RequestPendingEvents.
    virtual void ResetPendingEventsRequested() = 0;

    // Adds a new carrier profile under given euicc object using fake default
    // values for properties. If |state| is not pending then a corresponding
    // fake cellular service is also created in shill. The
    // |add_carrier_profile_behavior| parameter determines whether an associated
    // Shill service is created as well as whether the profile is added to the
    // EUICC immediately or only after RefreshInstalledProfiles() is called.
    // Returns the path to the newly added profile.
    virtual dbus::ObjectPath AddFakeCarrierProfile(
        const dbus::ObjectPath& euicc_path,
        hermes::profile::State state,
        const std::string& activation_code,
        AddCarrierProfileBehavior add_carrier_profile_behavior) = 0;

    // Adds a new carrier profile with given path and properties.
    virtual void AddCarrierProfile(
        const dbus::ObjectPath& path,
        const dbus::ObjectPath& euicc_path,
        const std::string& iccid,
        const std::string& name,
        const std::string& nickname,
        const std::string& service_provider,
        const std::string& activation_code,
        const std::string& network_service_path,
        hermes::profile::State state,
        hermes::profile::ProfileClass profile_class,
        AddCarrierProfileBehavior add_carrier_profile_behavior) = 0;

    // Remove a carrier profile with path |carrier_profile_path| from EUICC with
    // given |euicc_path|. Return true if successful.
    virtual bool RemoveCarrierProfile(
        const dbus::ObjectPath& euicc_path,
        const dbus::ObjectPath& carrier_profile_path) = 0;

    // Queues an error code that will be returned from a subsequent
    // method call.
    virtual void QueueHermesErrorStatus(HermesResponseStatus status) = 0;

    // Set delay for interactive methods.
    virtual void SetInteractiveDelay(base::TimeDelta delay) = 0;

    // Returns a valid fake activation code that can be used to install
    // a new fake carrier profile.
    virtual std::string GenerateFakeActivationCode() = 0;

    // Returns true when the last call to RefreshInstalledProfiles was requested
    // with |restore_slot| set to true.
    virtual bool GetLastRefreshProfilesRestoreSlotArg() = 0;
  };

  // Hermes Euicc properties.
  class Properties : public dbus::PropertySet {
   public:
    Properties(dbus::ObjectProxy* object_proxy,
               const PropertyChangedCallback& callback);
    ~Properties() override;

    dbus::Property<std::string>& eid() { return eid_; }
    dbus::Property<bool>& is_active() { return is_active_; }
    dbus::Property<std::vector<dbus::ObjectPath>>&
    installed_carrier_profiles() {
      return installed_carrier_profiles_;
    }
    dbus::Property<std::vector<dbus::ObjectPath>>& pending_carrier_profiles() {
      return pending_carrier_profiles_;
    }
    dbus::Property<int32_t>& physical_slot() { return physical_slot_; }

   private:
    // EID of the Euicc.
    dbus::Property<std::string> eid_;

    // Boolean that indicates whether this euicc is currently active.
    dbus::Property<bool> is_active_;

    // List of paths to carrier profiles currently installed on the device.
    dbus::Property<std::vector<dbus::ObjectPath>> installed_carrier_profiles_;

    // List of pending carrier profiles from SMDS available for
    // installation on this device.
    dbus::Property<std::vector<dbus::ObjectPath>> pending_carrier_profiles_;

    // Physical slot number of the Euicc.
    dbus::Property<int32_t> physical_slot_;
  };

  // Interface for observing Hermes Euicc changes.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when an euicc property changes.
    virtual void OnEuiccPropertyChanged(const dbus::ObjectPath& euicc_path,
                                        const std::string& property_name) {}

    // Called when an Euicc reset operation completes successfully.
    virtual void OnEuiccReset(const dbus::ObjectPath& euicc_path) {}
  };

  // Adds an observer for carrier profile lists changes on Hermes manager.
  virtual void AddObserver(Observer* observer);

  // Removes an observer for Hermes manager.
  virtual void RemoveObserver(Observer* observer);

  // Install a carrier profile in the Euicc at |euicc_path| with given
  // |activation_code| and |conirmation_code|. |confirmation_code| can be empty
  // if no confirmation is required by carrier. Returns the object path to the
  // carrier profile that was just installed.
  virtual void InstallProfileFromActivationCode(
      const dbus::ObjectPath& euicc_path,
      const std::string& activation_code,
      const std::string& confirmation_code,
      InstallCarrierProfileCallback callback) = 0;

  // Installs a pending profile with given |carrier_profile_path| in the Euicc
  // at |euicc_path|. |confirmation_code| can be empty if no confirmation is
  // required by carrier. Returns a response status indicating the install
  // result.
  virtual void InstallPendingProfile(
      const dbus::ObjectPath& euicc_path,
      const dbus::ObjectPath& carrier_profile_path,
      const std::string& confirmation_code,
      HermesResponseCallback callback) = 0;

  // Refreshes installed profiles for Euicc at |euicc_path|.
  // This updates installed profiles list prior to returning.
  // If |restore_slot| is true then SIM slot that was active prior to refreshing
  // is restored.
  virtual void RefreshInstalledProfiles(const dbus::ObjectPath& euicc_path,
                                        bool restore_slot,
                                        HermesResponseCallback callback) = 0;

  // Updates pending profiles for Euicc at |euicc_path| from the SMDS server
  // using the given |root_smds| server address. Passing an empty |root_smds|
  // will use default lpa.ds.gsma.com. This updates pending profiles list prior
  // to returning.
  virtual void RequestPendingProfiles(const dbus::ObjectPath& euicc_path,
                                      const std::string& root_smds,
                                      HermesResponseCallback callback) = 0;

  // Removes the carrier profile with the given |carrier_profile_path| from
  // the Euicc at |euicc_path|. Returns a response status indicating the result
  // of the operation.
  virtual void UninstallProfile(const dbus::ObjectPath& euicc_path,
                                const dbus::ObjectPath& carrier_profile_path,
                                HermesResponseCallback callback) = 0;

  // Erases all profiles on the Euicc at |euicc_path|. |reset_option| specifies
  // the type of reset operation that will be performed.
  virtual void ResetMemory(const dbus::ObjectPath& euicc_path,
                           hermes::euicc::ResetOptions reset_option,
                           HermesResponseCallback callback) = 0;

  // Returns properties for the Euicc with given |euicc_path|.
  virtual Properties* GetProperties(const dbus::ObjectPath& euicc_path) = 0;

  // Returns an instance of Hermes Euicc Test interface.
  virtual TestInterface* GetTestInterface() = 0;

  // Creates and initializes the global instance.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a global fake instance.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance.
  static HermesEuiccClient* Get();

 protected:
  HermesEuiccClient();
  virtual ~HermesEuiccClient();

  const base::ObserverList<Observer>::Unchecked& observers() {
    return observers_;
  }

 private:
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_HERMES_HERMES_EUICC_CLIENT_H_
