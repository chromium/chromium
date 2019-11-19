// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_SHILL_PROPERTY_HANDLER_H_
#define CHROMEOS_NETWORK_SHILL_PROPERTY_HANDLER_H_

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/shill/shill_property_changed_observer.h"
#include "chromeos/network/managed_state.h"
#include "chromeos/network/network_handler_callbacks.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace chromeos {

class ShillManagerClient;

namespace internal {

class ShillPropertyObserver;

// This class handles Shill calls and observers to reflect the state of the
// Shill Manager and its services and devices. It observes Shill.Manager and
// requests properties for new devices/networks. It takes a Listener in its
// constructor (e.g. NetworkStateHandler) that it calls when properties change
// (including once to set their initial state after Init() gets called).
// It also observes Shill.Service for all services in Manager.ServiceWatchList.
// This class must not outlive the ShillManagerClient instance.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) ShillPropertyHandler
    : public ShillPropertyChangedObserver,
      public base::SupportsWeakPtr<ShillPropertyHandler> {
 public:
  typedef std::map<std::string, std::unique_ptr<ShillPropertyObserver>>
      ShillPropertyObserverMap;

  class COMPONENT_EXPORT(CHROMEOS_NETWORK) Listener {
   public:
    // Called when the entries in a managed list have changed.
    virtual void UpdateManagedList(ManagedState::ManagedType type,
                                   const base::ListValue& entries) = 0;

    // Called when the properties for a managed state have changed.
    // |properties| is expected to be of type DICTIONARY.
    virtual void UpdateManagedStateProperties(
        ManagedState::ManagedType type,
        const std::string& path,
        const base::Value& properties) = 0;

    // Called when the list of profiles changes.
    virtual void ProfileListChanged() = 0;

    // Called when a property for a watched network service has changed.
    virtual void UpdateNetworkServiceProperty(
        const std::string& service_path,
        const std::string& key,
        const base::Value& value) = 0;

    // Called when a property for a watched device has changed.
    virtual void UpdateDeviceProperty(
        const std::string& device_path,
        const std::string& key,
        const base::Value& value) = 0;

    // Called when a watched network or device IPConfig property changes.
    virtual void UpdateIPConfigProperties(ManagedState::ManagedType type,
                                          const std::string& path,
                                          const std::string& ip_config_path,
                                          const base::Value& properties) = 0;

    // Called when the list of devices with portal check enabled changes.
    virtual void CheckPortalListChanged(
         const std::string& check_portal_list) = 0;

    // Called when a technology list changes.
    virtual void TechnologyListChanged() = 0;

    // Called when a managed state list has changed, after properties for any
    // new entries in the list have been received and
    // UpdateManagedStateProperties has been called for each new entry.
    virtual void ManagedStateListChanged(ManagedState::ManagedType type) = 0;

    // Called when the default network service changes.
    virtual void DefaultNetworkServiceChanged(
        const std::string& service_path) = 0;

   protected:
    virtual ~Listener() {}
  };

  explicit ShillPropertyHandler(Listener* listener);
  ~ShillPropertyHandler() override;

  // Sets up the observer and calls UpdateManagerProperties().
  void Init();

  // Requests all Manager properties. Called from Init() and any time
  // properties that do not signal changes might have been updated (e.g.
  // ServiceCompleteList).
  void UpdateManagerProperties();

  // Returns true if |technology| is available, enabled, etc.
  bool IsTechnologyAvailable(const std::string& technology) const;
  bool IsTechnologyEnabled(const std::string& technology) const;
  bool IsTechnologyEnabling(const std::string& technology) const;
  bool IsTechnologyDisabling(const std::string& technology) const;
  bool IsTechnologyProhibited(const std::string& technology) const;
  bool IsTechnologyUninitialized(const std::string& technology) const;

  // Asynchronously sets the enabled state for |technology|.
  // Note: Modifies Manager state. Calls |error_callback| on failure.
  void SetTechnologyEnabled(
      const std::string& technology,
      bool enabled,
      const network_handler::ErrorCallback& error_callback);

  // Asynchronously sets the prohibited state for every network technology
  // listed in |technologies|. Note: Modifies Manager state. Calls
  // |error_callback| on failure.
  void SetProhibitedTechnologies(
      const std::vector<std::string>& technologies,
      const network_handler::ErrorCallback& error_callback);

  // Sets the list of devices on which portal check is enabled.
  void SetCheckPortalList(const std::string& check_portal_list);

  // Sets the Manager.WakeOnLan property. Note: we do not track this state, we
  // only set it.
  void SetWakeOnLanEnabled(bool enabled);

  // Sets the HostName property. Note: we do not track this property, we
  // only set it.
  void SetHostname(const std::string& hostname);

  // Calls shill to enable/disable network bandwidth throttling. If |enabled|
  // is true, |upload_rate_kbits| and |download_rate_kbits| specify the rate
  // in kbits/s to throttle to. If |enabled| is false, throttling is disabled
  // and the rates are ignored.
  void SetNetworkThrottlingStatus(bool enabled,
                                  uint32_t upload_rate_kbits,
                                  uint32_t download_rate_kbits);

  // Sets Fast Transition status.
  void SetFastTransitionStatus(bool enabled);

  // Requests an immediate network scan for |type|.
  void RequestScanByType(const std::string& type) const;

  // Requests all properties for the service or device (called for new items).
  void RequestProperties(ManagedState::ManagedType type,
                         const std::string& path);

  // ShillPropertyChangedObserver overrides
  void OnPropertyChanged(const std::string& key,
                         const base::Value& value) override;

 private:
  typedef std::map<ManagedState::ManagedType, std::set<std::string>>
      TypeRequestMap;

  // Callback for dbus method fetching properties.
  void ManagerPropertiesCallback(DBusMethodCallStatus call_status,
                                 const base::DictionaryValue& properties);

  // Notifies the listener when a ManagedStateList has changed and all pending
  // updates have been received. |key| can either identify the list that
  // has changed or an empty string if multiple lists may have changed.
  void CheckPendingStateListUpdates(const std::string& key);

  // Called form OnPropertyChanged() and ManagerPropertiesCallback().
  void ManagerPropertyChanged(const std::string& key,
                              const base::Value& value);

  // Requests properties for new entries in the list for |type|.
  void UpdateProperties(ManagedState::ManagedType type,
                        const base::ListValue& entries);

  // Updates the Shill property observers to observe any entries for |type|.
  void UpdateObserved(ManagedState::ManagedType type,
                      const base::ListValue& entries);


  // Sets |*_technologies_| to contain only entries in |technologies|.
  void UpdateAvailableTechnologies(const base::ListValue& technologies);
  void UpdateEnabledTechnologies(const base::ListValue& technologies);
  void UpdateUninitializedTechnologies(const base::ListValue& technologies);

  void EnableTechnologyFailed(
      const std::string& technology,
      const network_handler::ErrorCallback& error_callback,
      const std::string& dbus_error_name,
      const std::string& dbus_error_message);

  void DisableTechnologyFailed(
      const std::string& technology,
      const network_handler::ErrorCallback& error_callback,
      const std::string& dbus_error_name,
      const std::string& dbus_error_message);

  // Called when Shill returns the properties for a service or device.
  void GetPropertiesCallback(ManagedState::ManagedType type,
                             const std::string& path,
                             DBusMethodCallStatus call_status,
                             const base::DictionaryValue& properties);

  // Callback invoked when a watched property changes. Calls appropriate
  // handlers and signals observers.
  void PropertyChangedCallback(ManagedState::ManagedType type,
                               const std::string& path,
                               const std::string& key,
                               const base::Value& value);

  // Request a single IPConfig object corresponding to |ip_config_path_value|
  // from Shill.IPConfigClient and trigger a call to UpdateIPConfigProperties
  // for the network or device corresponding to |type| and |path|.
  void RequestIPConfig(ManagedState::ManagedType type,
                       const std::string& path,
                       const base::Value& ip_config_path_value);

  // Request the IPConfig objects corresponding to entries in
  // |ip_config_list_value| from Shill.IPConfigClient and trigger a call to
  // UpdateIPConfigProperties with each object for the network or device
  // corresponding to |type| and |path|.
  void RequestIPConfigsList(ManagedState::ManagedType type,
                            const std::string& path,
                            const base::Value& ip_config_list_value);

  // Callback for getting the IPConfig property of a network or device. Handled
  // here instead of in NetworkState so that all asynchronous requests are done
  // in a single place (also simplifies NetworkState considerably).
  void GetIPConfigCallback(ManagedState::ManagedType type,
                           const std::string& path,
                           const std::string& ip_config_path,
                           DBusMethodCallStatus call_status,
                           const base::DictionaryValue& properties);

  void SetProhibitedTechnologiesEnforced(bool enforced);

  // Pointer to containing class (owns this)
  Listener* listener_;

  // Convenience pointer for ShillManagerClient
  ShillManagerClient* shill_manager_;

  // Pending update list for each managed state type
  TypeRequestMap pending_updates_;

  // List of states for which properties have been requested, for each managed
  // state type
  TypeRequestMap requested_updates_;

  // List of network services with Shill property changed observers
  ShillPropertyObserverMap observed_networks_;

  // List of network devices with Shill property changed observers
  ShillPropertyObserverMap observed_devices_;

  // Lists of available / enabled / uninitialized technologies
  std::set<std::string> available_technologies_;
  std::set<std::string> enabled_technologies_;
  std::set<std::string> enabling_technologies_;
  std::set<std::string> disabling_technologies_;
  std::set<std::string> prohibited_technologies_;
  std::set<std::string> uninitialized_technologies_;

  DISALLOW_COPY_AND_ASSIGN(ShillPropertyHandler);
};

}  // namespace internal
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_SHILL_PROPERTY_HANDLER_H_
