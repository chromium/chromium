// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_cert_migrator.h"

#include <cert.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/client_cert_util.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "dbus/object_path.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// Migrates each network of |networks| with an invalid or missing slot ID in
// their client certificate configuration.
//
// If a network with a client certificate configuration (i.e. a PKCS11 ID) is
// found, the configured client certificate is looked up.
// If the certificate is found, the currently configured slot ID (if any) is
// compared with the actual slot ID of the certificate and if required updated.
// If the certificate is not found, the client certificate configuration is
// removed.
//
// Only if necessary, a network will be notified.
class NetworkCertMigrator::MigrationTask
    : public base::RefCounted<MigrationTask> {
 public:
  MigrationTask(net::ScopedCERTCertificateList certs,
                const base::WeakPtr<NetworkCertMigrator>& cert_migrator)
      : certs_(std::move(certs)), cert_migrator_(cert_migrator) {}

  void Run(const NetworkStateHandler::NetworkStateList& networks) {
    // Request properties for each network that could be configured with a
    // client certificate.
    for (const NetworkState* network : networks) {
      if (network->security_class() != shill::kSecurity8021x &&
          network->type() != shill::kTypeVPN &&
          network->type() != shill::kTypeEthernetEap) {
        continue;
      }

      const std::string& service_path = network->path();
      // Defer migration of user networks to when the user certificate database
      // has finished loading.
      if (!CorrespondingCertificateDatabaseLoaded(network)) {
        VLOG(2) << "Skipping network cert migration of network "
                << service_path
                << " until the corresponding certificate database is loaded.";
        continue;
      }

      ShillServiceClient::Get()->GetProperties(
          dbus::ObjectPath(service_path),
          base::Bind(&network_handler::GetPropertiesCallback,
                     base::Bind(&MigrationTask::MigrateNetwork, this),
                     network_handler::ErrorCallback(), service_path));
    }
  }

  void MigrateNetwork(const std::string& service_path,
                      const base::DictionaryValue& properties) {
    if (!cert_migrator_) {
      VLOG(2) << "NetworkCertMigrator already destroyed. Aborting migration.";
      return;
    }

    base::DictionaryValue new_properties;
    MigrateClientCertProperties(service_path, properties, &new_properties);

    if (new_properties.empty())
      return;
    SendPropertiesToShill(service_path, new_properties);
  }

  void MigrateClientCertProperties(const std::string& service_path,
                                   const base::DictionaryValue& properties,
                                   base::DictionaryValue* new_properties) {
    int configured_slot_id = -1;
    std::string pkcs11_id;
    chromeos::client_cert::ConfigType config_type =
        chromeos::client_cert::CONFIG_TYPE_NONE;
    chromeos::client_cert::GetClientCertFromShillProperties(
        properties, &config_type, &configured_slot_id, &pkcs11_id);
    if (config_type == chromeos::client_cert::CONFIG_TYPE_NONE ||
        pkcs11_id.empty()) {
      return;
    }

    // OpenVPN configuration doesn't have a slot id to migrate.
    if (config_type == chromeos::client_cert::CONFIG_TYPE_OPENVPN)
      return;

    int real_slot_id = -1;
    CERTCertificate* cert =
        FindCertificateWithPkcs11Id(pkcs11_id, &real_slot_id);
    if (!cert) {
      LOG(WARNING) << "No matching cert found, removing the certificate "
                      "configuration from network " << service_path;
      chromeos::client_cert::SetEmptyShillProperties(config_type,
                                                     new_properties);
      return;
    }
    if (real_slot_id == -1) {
      LOG(WARNING) << "Found a certificate without slot id.";
      return;
    }

    if (cert && real_slot_id != configured_slot_id) {
      VLOG(1) << "Network " << service_path
              << " is configured with no or an incorrect slot id.";
      chromeos::client_cert::SetShillProperties(
          config_type, real_slot_id, pkcs11_id, new_properties);
    }
  }

  CERTCertificate* FindCertificateWithPkcs11Id(const std::string& pkcs11_id,
                                               int* slot_id) {
    *slot_id = -1;
    for (const net::ScopedCERTCertificate& cert : certs_) {
      int current_slot_id = -1;
      std::string current_pkcs11_id =
          NetworkCertLoader::GetPkcs11IdAndSlotForCert(cert.get(),
                                                       &current_slot_id);
      if (current_pkcs11_id == pkcs11_id) {
        *slot_id = current_slot_id;
        return cert.get();
      }
    }
    return nullptr;
  }

  void SendPropertiesToShill(const std::string& service_path,
                             const base::DictionaryValue& properties) {
    ShillServiceClient::Get()->SetProperties(
        dbus::ObjectPath(service_path), properties, base::DoNothing(),
        base::Bind(&LogError, service_path));
  }

  static void LogError(const std::string& service_path,
                       const std::string& error_name,
                       const std::string& error_message) {
    network_handler::ShillErrorCallbackFunction(
        "MigrationTask.SetProperties failed",
        service_path,
        network_handler::ErrorCallback(),
        error_name,
        error_message);
  }

 private:
  friend class base::RefCounted<MigrationTask>;
  virtual ~MigrationTask() = default;

  bool CorrespondingCertificateDatabaseLoaded(const NetworkState* network) {
    if (network->IsPrivate())
      return NetworkCertLoader::Get()->user_cert_database_load_finished();
    return NetworkCertLoader::Get()->initial_load_finished();
  }

  net::ScopedCERTCertificateList certs_;
  base::WeakPtr<NetworkCertMigrator> cert_migrator_;
};

NetworkCertMigrator::NetworkCertMigrator() : network_state_handler_(nullptr) {}

NetworkCertMigrator::~NetworkCertMigrator() {
  network_state_handler_->RemoveObserver(this, FROM_HERE);
  if (NetworkCertLoader::IsInitialized())
    NetworkCertLoader::Get()->RemoveObserver(this);
}

void NetworkCertMigrator::Init(NetworkStateHandler* network_state_handler) {
  DCHECK(network_state_handler);
  network_state_handler_ = network_state_handler;
  network_state_handler_->AddObserver(this, FROM_HERE);

  DCHECK(NetworkCertLoader::IsInitialized());
  NetworkCertLoader::Get()->AddObserver(this);
}

void NetworkCertMigrator::NetworkListChanged() {
  if (!NetworkCertLoader::Get()->initial_load_finished()) {
    VLOG(2) << "Certs not loaded yet.";
    return;
  }
  // Run the migration process to fix missing or incorrect slot ids of client
  // certificates.
  VLOG(2) << "Start certificate migration of network configurations.";
  scoped_refptr<MigrationTask> helper(base::MakeRefCounted<MigrationTask>(
      NetworkCertLoader::GetAllCertsFromNetworkCertList(
          NetworkCertLoader::Get()->client_certs()),
      weak_ptr_factory_.GetWeakPtr()));
  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Default(),
      true,   // only configured networks
      false,  // visible and not visible networks
      0,      // no count limit
      &networks);
  helper->Run(networks);
}

void NetworkCertMigrator::OnCertificatesLoaded() {
  NetworkListChanged();
}

}  // namespace chromeos
