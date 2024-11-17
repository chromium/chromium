// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONFIGURATION_HANDLER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONFIGURATION_HANDLER_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chromeos/ash/components/network/network_configuration_observer.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/dbus/common/dbus_callback.h"

namespace dbus {
class ObjectPath;
}

namespace ash {

inline constexpr char kTemporaryServiceConfiguredButNotUsable[] =
    "Temporary service configured but not usable";

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
// Note on callbacks: These methods are all asynchronous. There are two callback
// styles for these methods:
//
// Original: Separate callbacks for success (|callback|) and failure
// (|error_callback|). One or the other will be called. See ErrorCallback in
// network_handler_callbacks.h for more details.
//
// Preferred: A single callback is provided. This makes using OnceCallback
// simpler and ensures that the one callback will be called. An Optional<>
// result is provided which will be nullopt on failure. An Optional<string>
// error identifier may also be provided.

class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkConfigurationHandler
    : public NetworkStateHandlerObserver {
 public:
  NetworkConfigurationHandler(const NetworkConfigurationHandler&) = delete;
  NetworkConfigurationHandler& operator=(const NetworkConfigurationHandler&) =
      delete;

  ~NetworkConfigurationHandler() override;

  // Manages the observer list.
  void AddObserver(NetworkConfigurationObserver* observer);
  void RemoveObserver(NetworkConfigurationObserver* observer);

  // Gets the properties of the network with id |service_path|.
  void GetShillProperties(const std::string& service_path,
                          network_handler::ResultCallback callback);

  // Sets the properties of the network with id |service_path|. This means the
  // given properties will be merged with the existing settings, and it won't
  // clear any existing properties.
  void SetShillProperties(const std::string& service_path,
                          const base::Value::Dict& shill_properties,
                          base::OnceClosure callback,
                          network_handler::ErrorCallback error_callback);

  // Removes the properties with the given property paths. If any of them are
  // unable to be cleared, the |error_callback| will only be run once with
  // accumulated information about all of the errors as a list attached to the
  // "errors" key of the error data, and the |callback| will not be run, even
  // though some of the properties may have been cleared. If there are no
  // errors, |callback| will be run.
  void ClearShillProperties(const std::string& service_path,
                            const std::vector<std::string>& property_paths,
                            base::OnceClosure callback,
                            network_handler::ErrorCallback error_callback);

  // Creates a network with the given |properties| in the specified Shill
  // profile, and returns the new service_path to |callback| if successful.
  // |callback| will only be called after the property update has been reflected
  // in NetworkStateHandler.
  // kProfileProperty must be set in |properties|. This may also be used to
  // update an existing matching configuration, see Shill documentation for
  // Manager.ConfigureServiceForProfile. NOTE: Normally
  // ManagedNetworkConfigurationHandler should be used to call
  // CreateConfiguration. This will set GUID if not provided.
  void CreateShillConfiguration(const base::Value::Dict& shill_properties,
                                network_handler::ServiceResultCallback callback,
                                network_handler::ErrorCallback error_callback);

  using RemoveConfirmer =
      base::RepeatingCallback<bool(const std::string& guid,
                                   const std::string& profile_path)>;
  // Removes the network |service_path| from any profiles that include it. If
  // |remove_confirmer| is provided, it will be used to confirm the remove
  // operation and only entries that evaluate to true by applying the confirmer
  // will be removed.
  void RemoveConfiguration(const std::string& service_path,
                           std::optional<RemoveConfirmer> remove_confirmer,
                           base::OnceClosure callback,
                           network_handler::ErrorCallback error_callback);

  // Removes the network |service_path| from the profile that contains its
  // currently active configuration.
  void RemoveConfigurationFromCurrentProfile(
      const std::string& service_path,
      base::OnceClosure callback,
      network_handler::ErrorCallback error_callback);

  // Changes the profile for the network |service_path| to |profile_path|.
  void SetNetworkProfile(const std::string& service_path,
                         const std::string& profile_path,
                         base::OnceClosure callback,
                         network_handler::ErrorCallback error_callback);

  // Changes the value of a shill manager property.
  void SetManagerProperty(const std::string& property_name,
                          const base::Value& value);

  // NetworkStateHandlerObserver
  void NetworkListChanged() override;
  void OnShuttingDown() override;

  // Construct and initialize an instance for testing.
  static std::unique_ptr<NetworkConfigurationHandler> InitializeForTest(
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

  // Called when a configuration completes. This will use
  // NotifyConfigurationCompleted to defer notifying observers that a
  // configuration was completed and invoking |callback| until the cached state
  // (NetworkStateHandler) to update before triggering the callback.
  void ConfigurationCompleted(const std::string& profile_path,
                              const std::string& guid,
                              base::Value::Dict configure_properties,
                              network_handler::ServiceResultCallback callback,
                              const dbus::ObjectPath& service_path);

  // Used by ConfigurationCompleted to defer notifying observers and invoking
  // the provided callback.
  void NotifyConfigurationCompleted(
      network_handler::ServiceResultCallback callback,
      const std::string& service_path,
      const std::string& guid);

  void ConfigurationFailed(network_handler::ErrorCallback error_callback,
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
                                  base::OnceClosure callback);

  // Set the Name and GUID properties correctly and Invoke |callback|.
  void GetPropertiesCallback(network_handler::ResultCallback callback,
                             const std::string& service_path,
                             std::optional<base::Value::Dict> properties);

  // Invoke |callback| and inform NetworkStateHandler to request an update
  // for the service after setting properties.
  void SetPropertiesSuccessCallback(const std::string& service_path,
                                    base::Value::Dict set_properties,
                                    base::OnceClosure callback);
  void SetPropertiesErrorCallback(const std::string& service_path,
                                  network_handler::ErrorCallback error_callback,
                                  const std::string& dbus_error_name,
                                  const std::string& dbus_error_message);

  // Invoke |callback| and inform NetworkStateHandler to request an update
  // for the service after clearing properties.
  void ClearPropertiesSuccessCallback(const std::string& service_path,
                                      const std::vector<std::string>& names,
                                      base::OnceClosure callback,
                                      const base::Value::List& result);
  void ClearPropertiesErrorCallback(
      const std::string& service_path,
      network_handler::ErrorCallback error_callback,
      const std::string& dbus_error_name,
      const std::string& dbus_error_message);

  // Removes network configuration for |service_path| from the profile specified
  // by |profile_path|. If |profile_path| is not set, the network is removed
  // from all the profiles that include it. If |remove_confirmer| is provided,
  // it will be used to confirm the remove operation and only entries that
  // evaluate to true by applying the confirmer will be removed.
  void RemoveConfigurationFromProfile(
      const std::string& service_path,
      const std::string& profile_path,
      std::optional<RemoveConfirmer> remove_confirmer,
      base::OnceClosure callback,
      network_handler::ErrorCallback error_callback);

  // Unowned associated Network*Handlers (global or test instance).
  raw_ptr<NetworkStateHandler> network_state_handler_;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
  raw_ptr<NetworkDeviceHandler, DanglingUntriaged> network_device_handler_;

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
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CONFIGURATION_HANDLER_H_
