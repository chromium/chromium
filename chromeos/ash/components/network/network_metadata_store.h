// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_METADATA_STORE_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_METADATA_STORE_H_

#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/cellular_utils.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_observer.h"
#include "chromeos/ash/components/network/network_connection_observer.h"
#include "chromeos/ash/components/network/network_metadata_observer.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/ash/components/network/text_message_suppression_state.h"

class PrefService;
class PrefRegistrySimple;

namespace base {
class TimeDelta;
}

namespace ash {

class NetworkConfigurationHandler;
class NetworkConnectionHandler;
class NetworkStateHandler;

// Stores metadata about networks using the UserProfilePrefStore for networks
// that are on the local profile and the LocalStatePrefStore for shared
// networks.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkMetadataStore
    : public NetworkConnectionObserver,
      public NetworkConfigurationObserver,
      public NetworkStateHandlerObserver,
      public LoginState::Observer {
 public:
  NetworkMetadataStore(
      NetworkConfigurationHandler* network_configuration_handler,
      NetworkConnectionHandler* network_connection_handler,
      NetworkStateHandler* network_state_handler,
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      PrefService* profile_pref_service,
      PrefService* device_pref_service,
      bool is_enterprise_managed);

  ~NetworkMetadataStore() override;

  // Registers preferences used by this class in the provided |registry|.  This
  // should be called for both the Profile registry and the Local State registry
  // prior to using this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // LoginState::Observer overrides.
  void LoggedInStateChanged() override;

  // NetworkConnectionObserver::
  void ConnectSucceeded(const std::string& service_path) override;
  void ConnectFailed(const std::string& service_path,
                     const std::string& error_name) override;

  // NetworkConfigurationObserver::
  void OnConfigurationCreated(const std::string& service_path,
                              const std::string& guid) override;
  void OnConfigurationModified(
      const std::string& service_path,
      const std::string& guid,
      const base::Value::Dict* set_properties) override;
  void OnConfigurationRemoved(const std::string& service_path,
                              const std::string& guid) override;

  // NetworkStateHandlerObserver::
  void NetworkListChanged() override;

  // Records that the network was added by sync.
  void SetIsConfiguredBySync(const std::string& network_guid);

  // Returns the timestamp when the network was last connected to, or 0 if it
  // has never had a successful connection.
  base::TimeDelta GetLastConnectedTimestamp(const std::string& network_guid);
  void SetLastConnectedTimestamp(const std::string& network_guid,
                                 const base::TimeDelta& timestamp);

  // Returns the timestamp when the network was created, rounded to the
  // nearest day. Will clear the timestamp of WiFi networks two weeks
  // after creation and always return base::Time::UnixEpoch() for non-
  // WiFi networks.
  base::Time UpdateAndRetrieveWiFiTimestamp(const std::string& network_guid);

  // Networks which were added directly from sync data will return true.
  bool GetIsConfiguredBySync(const std::string& network_guid);

  // Networks which were created by the logged in user will return true.
  bool GetIsCreatedByUser(const std::string& network_guid);

  // When another user modifies a watched field.
  bool GetIsFieldExternallyModified(const std::string& network_guid,
                                    const std::string& field);

  // If a connection to a Wi-Fi network fails because of a bad password before
  // it has ever connected successfully, then this will return true. Once there
  // has been a successful connection with the currently saved password, this
  // will always return false.
  bool GetHasBadPassword(const std::string& network_guid);

  // Stores a list of user-entered APN entries for a cellular network. Takes
  // ownership of |list|.
  void SetCustomApnList(const std::string& network_guid,
                        base::Value::List list);

  // Returns custom apn list for cellular network with given guid. Returns
  // nullptr if no pref exists for |network_guid|.
  virtual const base::Value::List* GetCustomApnList(
      const std::string& network_guid);

  // Returns the pre APN revamp custom apns for a cellular network with given
  // guid. Returns nullptr if no pref exists for |network_guid|. Can only be
  // called if the APN Revamp flag is enabled.
  virtual const base::Value::List* GetPreRevampCustomApnList(
      const std::string& network_guid);

  // When the active user is the device owner and its the first login, this
  // marks networks that were added in OOBE to the user's list.
  void OwnSharedNetworksOnFirstUserLogin();

  // Sets the day of the month on which traffic counters are automatically
  // reset.
  void SetDayOfTrafficCountersAutoReset(const std::string& network_guid,
                                        const std::optional<int>& day);

  // Returns the day of the month on which traffic counters are automatically
  // reset. Returns nullptr if no pref exists for |network_guid|.
  const base::Value* GetDayOfTrafficCountersAutoReset(
      const std::string& network_guid);

  // Records if the default network is configured to use secure DNS template
  // URIs which contain user or device identifiers.
  void SetSecureDnsTemplatesWithIdentifiersActive(bool active);

  // Returns whether the default network is configured to use secure DNS
  // template URIs which contain user or device identifiers.
  bool secure_dns_templates_with_identifiers_active() const {
    return secure_dns_templates_with_identifiers_active_;
  }

  // Sets user suppression state to configure text message notifications.
  virtual void SetUserTextMessageSuppressionState(
      const std::string& network_guid,
      const UserTextMessageSuppressionState& state);

  // Returns the user set text message suppression state. When no user state has
  // been configured this will return |TextMessageSuppressionState::kAllow|
  // which will default to allowing text message notifications.
  virtual UserTextMessageSuppressionState GetUserTextMessageSuppressionState(
      const std::string& network_guid);

  // Sets whether the deviceReportXDREvents policy is enabled.
  void SetReportXdrEventsEnabled(bool enabled);

  // Returns whether the deviceReportXDREvents policy is enabled.
  bool report_xdr_events_enabled() { return report_xdr_events_enabled_; }

  // Manage observers.
  void AddObserver(NetworkMetadataObserver* observer);
  void RemoveObserver(NetworkMetadataObserver* observer);

  base::WeakPtr<NetworkMetadataStore> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void RemoveNetworkFromPref(const std::string& network_guid,
                             PrefService* pref_service);
  void SetPref(const std::string& network_guid,
               const std::string& key,
               base::Value value);
  const base::Value* GetPref(const std::string& network_guid,
                             const std::string& key);
  const base::Value::List* GetListPref(const std::string& network_guid,
                                       const std::string& key);
  void UpdateExternalModifications(const std::string& network_guid,
                                   const std::string& field);
  void LogHiddenNetworkAge();
  void FixSyncedHiddenNetworks();
  bool HasFixedHiddenNetworks();

  // Sets the owner metadata when there is an active user, otherwise a no-op.
  void SetIsCreatedByUser(const std::string& network_guid);
  void OnDisableHiddenError(const std::string& error_name);

  base::ObserverList<NetworkMetadataObserver> observers_;
  raw_ptr<NetworkConfigurationHandler> network_configuration_handler_;
  raw_ptr<NetworkConnectionHandler> network_connection_handler_;
  raw_ptr<NetworkStateHandler> network_state_handler_;
  raw_ptr<ManagedNetworkConfigurationHandler>
      managed_network_configuration_handler_;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
  raw_ptr<PrefService> profile_pref_service_;
  raw_ptr<PrefService> device_pref_service_;
  bool is_enterprise_managed_;
  bool has_profile_loaded_ = false;
  bool secure_dns_templates_with_identifiers_active_ = false;
  bool report_xdr_events_enabled_ = false;
  base::WeakPtrFactory<NetworkMetadataStore> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_METADATA_STORE_H_
