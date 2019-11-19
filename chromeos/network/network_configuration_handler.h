// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_CONFIGURATION_HANDLER_H_
#define CHROMEOS_NETWORK_NETWORK_CONFIGURATION_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/network/network_configuration_observer.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace dbus {
class ObjectPath;
}

namespace chromeos {

// The NetworkConfigurationHandler class is used to create and configure
// networks in ChromeOS. It mostly calls through to the Shill service API, and
// most calls are asynchronous for that reason. No calls will block on DBus
// calls.
//
// This is owned and it's lifetime managed by the Chrome startup code. It's
// basically a singleton, but with explicit lifetime management.
//
// For accessing lists of remembered networks, and other state information, see
// the class NetworkStateHandler.
//
// Note on callbacks: Because all the functions here are meant to be
// asynchronous, they all take a |callback| of some type, and an
// |error_callback|. When the operation succeeds, |callback| will be called, and
// when it doesn't, |error_callback| will be called with information about the
// error, including a symbolic name for the error and often some error message
// that is suitable for logging. None of the error message text is meant for
// user consumption.  Both |callback| and |error_callback| are permitted to be
// null callbacks.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkConfigurationHandler
    : public NetworkStateHandlerObserver {
 public:
  ~NetworkConfigurationHandler() override;

  // Manages the observer list.
  void AddObserver(NetworkConfigurationObserver* observer);
  void RemoveObserver(NetworkConfigurationObserver* observer);

  // Gets the properties of the network with id |service_path|. See note on
  // |callback| and |error_callback|, in class description above.
  void GetShillProperties(
      const std::string& service_path,
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback);

  // Sets the properties of the network with id |service_path|. This means the
  // given properties will be merged with the existing settings, and it won't
  // clear any existing properties.
  void SetShillProperties(const std::string& service_path,
                          const base::DictionaryValue& shill_properties,
                          const base::Closure& callback,
                          const network_handler::ErrorCallback& error_callback);

  // Removes the properties with the given property paths. If any of them are
  // unable to be cleared, the |error_callback| will only be run once with
  // accumulated information about all of the errors as a list attached to the
  // "errors" key of the error data, and the |callback| will not be run, even
  // though some of the properties may have been cleared. If there are no
  // errors, |callback| will be run.
  void ClearShillProperties(
      const std::string& service_path,
      const std::vector<std::string>& property_paths,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback);

  // Creates a network with the given |properties| in the specified Shill
  // profile, and returns the new service_path to |callback| if successful.
  // |callback| will only be called after the property update has been reflected
  // in NetworkStateHandler.
  // kProfileProperty must be set in |properties|. This may also be used to
  // update an existing matching configuration, see Shill documentation for
  // Manager.ConfigureServiceForProfile. NOTE: Normally
  // ManagedNetworkConfigurationHandler should be used to call
  // CreateConfiguration. This will set GUID if not provided.
  void CreateShillConfiguration(
      const base::DictionaryValue& shill_properties,
      const network_handler::ServiceResultCallback& callback,
      const network_handler::ErrorCallback& error_callback);

  // Removes the network |service_path| from any profiles that include it.
  void RemoveConfiguration(
      const std::string& service_path,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback);

  // Removes the network |service_path| from the profile that contains its
  // currently active configuration.
  void RemoveConfigurationFromCurrentProfile(
      const std::string& service_path,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback);

  // Changes the profile for the network |service_path| to |profile_path|.
  void SetNetworkProfile(const std::string& service_path,
                         const std::string& profile_path,
                         const base::Closure& callback,
                         const network_handler::ErrorCallback& error_callback);

  // Changes the value of a shill manager property.
  void SetManagerProperty(const std::string& property_name,
                          const base::Value& value,
                          const base::Closure& callback,
                          const network_handler::ErrorCallback& error_callback);

  // NetworkStateHandlerObserver
  void NetworkListChanged() override;
  void OnShuttingDown() override;

  // Construct and initialize an instance for testing.
  static NetworkConfigurationHandler* InitializeForTest(
      NetworkStateHandler* network_state_handler,
      NetworkDeviceHandler* network_device_handler);

 private:
  friend class ClientCertResolverTest;
  friend class NetworkHandler;
  friend class NetworkConfigurationHandlerTest;
  friend class NetworkConfigurationHandlerMockTest;
  class ProfileEntryDeleter;

  NetworkConfigurationHandler();
  void Init(NetworkStateHandler* network_state_handler,
            NetworkDeviceHandler* network_device_handler);

  // Called when a configuration completes. This will wait for the cached
  // state (NetworkStateHandler) to update before triggering the callback.
  void ConfigurationCompleted(
      const std::string& profile_path,
      std::unique_ptr<base::DictionaryValue> configure_properties,
      const network_handler::ServiceResultCallback& callback,
      const dbus::ObjectPath& service_path);

  void ConfigurationFailed(const network_handler::ErrorCallback& error_callback,
                           const std::string& dbus_error_name,
                           const std::string& dbus_error_message);

  // Called from ProfileEntryDeleter instances when they complete causing
  // this class to delete the instance.
  void ProfileEntryDeleterCompleted(const std::string& service_path,
                                    const std::string& guid,
                                    bool success);
  bool PendingProfileEntryDeleterForTest(const std::string& service_path) {
    return profile_entry_deleters_.count(service_path);
  }

  // Callback after moving a network configuration.
  void SetNetworkProfileCompleted(const std::string& service_path,
                                  const std::string& profile_path,
                                  const base::Closure& callback);

  // Set the Name and GUID properties correctly and Invoke |callback|.
  void GetPropertiesCallback(
      const network_handler::DictionaryResultCallback& callback,
      const network_handler::ErrorCallback& error_callback,
      const std::string& service_path,
      DBusMethodCallStatus call_status,
      const base::DictionaryValue& properties);

  // Invoke |callback| and inform NetworkStateHandler to request an update
  // for the service after setting properties.
  void SetPropertiesSuccessCallback(
      const std::string& service_path,
      std::unique_ptr<base::DictionaryValue> set_properties,
      const base::Closure& callback);
  void SetPropertiesErrorCallback(
      const std::string& service_path,
      const network_handler::ErrorCallback& error_callback,
      const std::string& dbus_error_name,
      const std::string& dbus_error_message);

  // Invoke |callback| and inform NetworkStateHandler to request an update
  // for the service after clearing properties.
  void ClearPropertiesSuccessCallback(
      const std::string& service_path,
      const std::vector<std::string>& names,
      const base::Closure& callback,
      const base::ListValue& result);
  void ClearPropertiesErrorCallback(
      const std::string& service_path,
      const network_handler::ErrorCallback& error_callback,
      const std::string& dbus_error_name,
      const std::string& dbus_error_message);

  // Removes network configuration for |service_path| from the profile specified
  // by |profile_path|. If |profile_path| is not set, the network is removed
  // from all the profiles that include it.
  void RemoveConfigurationFromProfile(
      const std::string& service_path,
      const std::string& profile_path,
      const base::Closure& callback,
      const network_handler::ErrorCallback& error_callback);

  // Unowned associated Network*Handlers (global or test instance).
  NetworkStateHandler* network_state_handler_;
  NetworkDeviceHandler* network_device_handler_;

  // Map of in-progress deleter instances.
  std::map<std::string, std::unique_ptr<ProfileEntryDeleter>>
      profile_entry_deleters_;

  // Map of configuration callbacks to run once the service becomes available
  // in the NetworkStateHandler cache. This is a multimap because there can be
  // multiple callbacks for the same network that have to be notified.
  std::multimap<std::string, network_handler::ServiceResultCallback>
      configure_callbacks_;

  base::ObserverList<NetworkConfigurationObserver, true>::Unchecked observers_;

  base::WeakPtrFactory<NetworkConfigurationHandler> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NetworkConfigurationHandler);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CONFIGURATION_HANDLER_H_
