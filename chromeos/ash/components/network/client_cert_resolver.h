// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_CLIENT_CERT_RESOLVER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_CLIENT_CERT_RESOLVER_H_

#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/client_cert_util.h"
#include "chromeos/ash/components/network/network_cert_loader.h"
#include "chromeos/ash/components/network/network_policy_observer.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"

namespace base {
class Clock;
class Value;
}  // namespace base

namespace ash {

class ManagedNetworkConfigurationHandler;
class NetworkState;

namespace internal {
struct NetworkAndMatchingCert;
}  // namespace internal

// Observes the known networks. If a network is configured with a client
// certificate pattern, this class searches for a matching client certificate.
// Each time it finds a match, it configures the network accordingly.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) ClientCertResolver
    : public NetworkStateHandlerObserver,
      public NetworkCertLoader::Observer,
      public NetworkPolicyObserver {
 public:
  class Observer {
   public:
    Observer& operator=(const Observer&) = delete;

    // Called every time resolving of client certificate patterns finishes,
    // no resolve requests are pending and no tasks are running.
    // |network_properties_changed| will be true if any network properties were
    // changed by this resolver since the last notification.
    virtual void ResolveRequestCompleted(bool network_properties_changed) = 0;

   protected:
    virtual ~Observer() {}
  };

  ClientCertResolver();

  ClientCertResolver(const ClientCertResolver&) = delete;
  ClientCertResolver& operator=(const ClientCertResolver&) = delete;

  ~ClientCertResolver() override;

  void Init(NetworkStateHandler* network_state_handler,
            ManagedNetworkConfigurationHandler* managed_network_config_handler);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if any resolve tasks are running. Every time a task finishes
  // and no further requests are pending, a notification is sent, see
  // |Observer|.
  bool IsAnyResolveTaskRunning() const;

  // Sets the clock for testing. This clock is used when checking the
  // certificates for expiration.
  void SetClockForTesting(base::Clock* clock);

  // Returns true and sets the Shill properties that have to be configured in
  // |shill_properties| if the client certificate could be resolved according to
  // |client_cert_config|.
  // Returns false otherwise and sets empty Shill properties to clear the
  // certificate configuration.
  // Note that it uses the global clock when checking the certificates for
  // expiration.
  static bool ResolveClientCertificateSync(
      const client_cert::ConfigType client_cert_type,
      const client_cert::ClientCertConfig& client_cert_config,
      base::Value::Dict* shill_properties);

  // Allows overwriting the function which gets the client certificate
  // provisioning profile id of a certificate. This is necessary for unit tests,
  // because there we use an NSS soft token which does not support the custom
  // attributes used for storing the id. Calling this will overwrite the
  // behavior until the returned ScopedClosureRunner is destructed, which will
  // reset to the original behavior.
  using ProvisioningProfileIdGetter =
      base::RepeatingCallback<std::string(CERTCertificate* cert)>;
  static base::ScopedClosureRunner SetProvisioningIdForCertGetterForTesting(
      ProvisioningProfileIdGetter getter);

 private:
  // NetworkStateHandlerObserver overrides
  void NetworkListChanged() override;
  void NetworkConnectionStateChanged(const NetworkState* network) override;

  // NetworkCertLoader::Observer overrides
  void OnCertificatesLoaded() override;

  // NetworkPolicyObserver overrides
  void PolicyAppliedToNetwork(const std::string& service_path) override;

  // Check which networks of |networks| are configured with a client certificate
  // pattern. Search for certificates, on the worker thread, and configure the
  // networks for which a matching cert is found (see ConfigureCertificates).
  void ResolveNetworks(const NetworkStateHandler::NetworkStateList& networks);

  // Resolves certificates for the pending networks. This will always trigger a
  // ResolveRequestCompleted notification, even if the queue is empty.
  void ResolvePendingNetworks();

  // |matches| contains networks for which a matching certificate was found.
  // Configures these networks.
  void ConfigureCertificates(
      std::vector<internal::NetworkAndMatchingCert> matches);

  // Trigger a ResolveRequestCompleted event on all observers.
  void NotifyResolveRequestCompleted();

  // Returns Time::Now() unless a mock clock has been installed with
  // SetClockForTesting, in which case the time according to that clock is used
  // instead.
  base::Time Now() const;

  base::ObserverList<Observer, true>::Unchecked observers_;

  // Tracks which network configurations ClientCertResolver is aware of, to be
  // able to detect newly created networks for which certificate resolution may
  // be necessary. The elements in the set are shill service paths.
  base::flat_set<std::string> known_networks_service_paths_;

  // The list of network paths that still have to be resolved.
  std::set<std::string> queued_networks_to_resolve_;

  // True if currently a resolve task is running.
  bool resolve_task_running_;

  // True if any network properties were changed since the last notification to
  // observers.
  bool network_properties_changed_;

  // Unowned associated (global or test) instance.
  raw_ptr<NetworkStateHandler> network_state_handler_;
  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  // Unowned associated (global or test) instance.
  raw_ptr<ManagedNetworkConfigurationHandler> managed_network_config_handler_;

  // Can be set for testing.
  raw_ptr<base::Clock> testing_clock_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<ClientCertResolver> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_CLIENT_CERT_RESOLVER_H_
