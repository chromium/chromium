// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/client_cert_resolver.h"

#include <cert.h>
#include <certt.h>  // for (SECCertUsageEnum) certUsageAnyCA
#include <pk11pub.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/task/post_task.h"
#include "base/time/clock.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/certificate_helper.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/onc/onc_certificate_pattern.h"
#include "chromeos/network/onc/variable_expander.h"
#include "components/onc/onc_constants.h"
#include "crypto/scoped_nss_types.h"
#include "dbus/object_path.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Describes a resolved client certificate along with the EAP identity field.
struct MatchingCert {
  MatchingCert() {}

  MatchingCert(const std::string& pkcs11_id,
               int key_slot_id,
               const std::string& configured_identity)
      : pkcs11_id(pkcs11_id),
        key_slot_id(key_slot_id),
        identity(configured_identity) {}

  bool operator==(const MatchingCert& other) const {
    return pkcs11_id == other.pkcs11_id && key_slot_id == other.key_slot_id &&
           identity == other.identity;
  }

  // The id of the matching certificate.
  std::string pkcs11_id;

  // The id of the slot containing the certificate and the private key.
  int key_slot_id = -1;

  // The ONC WiFi.EAP.Identity field can contain variables like
  // ${CERT_SAN_EMAIL} which are expanded by ClientCertResolver.
  // |identity| stores a copy of this string after the substitution
  // has been done.
  std::string identity;
};

// Describes a network that is configured with |client_cert_config|, which
// includes the certificate config.
struct NetworkAndCertConfig {
  NetworkAndCertConfig(const std::string& network_path,
                       const client_cert::ClientCertConfig& client_cert_config)
      : service_path(network_path), cert_config(client_cert_config) {}

  std::string service_path;
  client_cert::ClientCertConfig cert_config;
};

// The certificate resolving status of a known network that needs certificate
// pattern resolution.
enum class ResolveStatus { kResolving, kResolved };

// Returns substitutions based on |cert|'s contents to be used in a
// VariableExpander.
std::map<std::string, std::string> GetSubstitutionsForCert(
    CERTCertificate* cert) {
  std::map<std::string, std::string> substitutions;

  {
    std::vector<std::string> names;
    net::x509_util::GetRFC822SubjectAltNames(cert, &names);
    // Currently, we only use the first specified RFC8222
    // SubjectAlternativeName.
    std::string firstSANEmail;
    if (!names.empty())
      firstSANEmail = names[0];
    substitutions[::onc::substitutes::kCertSANEmail] = firstSANEmail;
  }

  {
    std::vector<std::string> names;
    net::x509_util::GetUPNSubjectAltNames(cert, &names);
    // Currently, we only use the first specified UPN SubjectAlternativeName.
    std::string firstSANUPN;
    if (!names.empty())
      firstSANUPN = names[0];
    substitutions[::onc::substitutes::kCertSANUPN] = firstSANUPN;
  }

  substitutions[::onc::substitutes::kCertSubjectCommonName] =
      certificate::GetCertAsciiSubjectCommonName(cert);

  return substitutions;
}

// Returns true if |client_cert_config| specifies a pattern or reference, i.e.
// if client certificate resolution should be attempted.
bool ShouldResolveCert(
    const client_cert::ClientCertConfig& client_cert_config) {
  return client_cert_config.client_cert_type == ::onc::client_cert::kPattern ||
         client_cert_config.client_cert_type == ::onc::client_cert::kRef;
}

}  // namespace

namespace internal {

// Describes the resolve status for a network, and if resolving already
// completed, also holds the matched certificate.
struct MatchingCertAndResolveStatus {
  // kResolving if client cert resolution is pending, kResolved if client cert
  // resolution has been completed for the network.
  ResolveStatus resolve_status = ResolveStatus::kResolving;

  // This is set to the last resolved client certificate or nullopt if no
  // matching certificate has been found when |resolve_status| is kResolved.
  // This is also used to determine if re-resolving a network actually changed
  // any properties.
  base::Optional<MatchingCert> matching_cert;
};

// Describes a network |network_path| and the client cert resolution result.
struct NetworkAndMatchingCert {
  NetworkAndMatchingCert(const NetworkAndCertConfig& network_and_cert_config,
                         base::Optional<MatchingCert> matching_cert)
      : service_path(network_and_cert_config.service_path),
        cert_config_type(network_and_cert_config.cert_config.location),
        matching_cert(matching_cert) {}

  std::string service_path;
  client_cert::ConfigType cert_config_type;

  // The resolved certificate, or |nullopt| if no matching certificate has been
  // found.
  base::Optional<MatchingCert> matching_cert;
};

}  // namespace internal

using internal::MatchingCertAndResolveStatus;
using internal::NetworkAndMatchingCert;

namespace {

// Returns the nickname of the private key for certificate |cert|, if such a
// private key is installed. Note that this is not a cheap operation: it
// iterates all tokens and attempts to look up the private key.
// A return value of |base::nullopt| means that no private key could be found
// for |cert|.
// If a private key could be found for |cert| but it did not have a nickname,
// will return the empty string.
base::Optional<std::string> GetPrivateKeyNickname(CERTCertificate* cert) {
  crypto::ScopedSECKEYPrivateKey key(
      PK11_FindKeyByAnyCert(cert, nullptr /* wincx */));
  if (!key)
    return base::nullopt;

  std::string key_nickname;
  char* nss_key_nickname = PK11_GetPrivateKeyNickname(key.get());
  if (nss_key_nickname) {
    key_nickname = nss_key_nickname;
    PORT_Free(nss_key_nickname);
  }
  return key_nickname;
}

// Describes a certificate which is issued by |issuer| (encoded as PEM).
// |issuer| can be empty if no issuer certificate is found in the database.
struct CertAndIssuer {
  CertAndIssuer(net::ScopedCERTCertificate certificate,
                const std::string& issuer,
                const std::string& key_nickname)
      : cert(std::move(certificate)),
        pem_encoded_issuer(issuer),
        private_key_nickname(key_nickname) {}

  net::ScopedCERTCertificate cert;
  std::string pem_encoded_issuer;
  // The nickname of the private key associated with |cert|. This is used
  // for resolving ClientCertRef ONC references, using the fact that
  // |CertificateImporterImpl| sets the private key nickname to the
  // ONC-specified GUID.
  std::string private_key_nickname;
};

bool CompareCertExpiration(const CertAndIssuer& a, const CertAndIssuer& b) {
  base::Time a_not_after;
  base::Time b_not_after;
  net::x509_util::GetValidityTimes(a.cert.get(), nullptr, &a_not_after);
  net::x509_util::GetValidityTimes(b.cert.get(), nullptr, &b_not_after);
  return a_not_after > b_not_after;
}

// A unary predicate that returns true if the given CertAndIssuer matches the
// given certificate config.
struct MatchCertWithCertConfig {
  explicit MatchCertWithCertConfig(
      const client_cert::ClientCertConfig& client_cert_config)
      : cert_config(client_cert_config) {}

  bool operator()(const CertAndIssuer& cert_and_issuer) {
    if (cert_config.client_cert_type == ::onc::client_cert::kPattern) {
      // Allow UTF-8 inside PrintableStrings in client certificates. See
      // crbug.com/770323 and crbug.com/788655.
      net::X509Certificate::UnsafeCreateOptions options;
      options.printable_string_is_utf8 = true;
      scoped_refptr<net::X509Certificate> x509_cert =
          net::x509_util::CreateX509CertificateFromCERTCertificate(
              cert_and_issuer.cert.get(), {}, options);
      if (!x509_cert)
        return false;
      return cert_config.pattern.Matches(*x509_cert,
                                         cert_and_issuer.pem_encoded_issuer);
    }

    if (cert_config.client_cert_type == ::onc::client_cert::kRef) {
      // This relies on the fact that |CertificateImporterImpl| sets the
      // nickname for the imported private key to the GUID.
      return cert_config.guid == cert_and_issuer.private_key_nickname;
    }

    NOTREACHED();
    return false;
  }

  const client_cert::ClientCertConfig cert_config;
};

// Lookup the issuer certificate of |cert|. If it is available, return the PEM
// encoding of that certificate. Otherwise return the empty string.
std::string GetPEMEncodedIssuer(CERTCertificate* cert) {
  net::ScopedCERTCertificate issuer_handle(
      CERT_FindCertIssuer(cert, PR_Now(), certUsageAnyCA));
  if (!issuer_handle) {
    VLOG(1) << "Couldn't find an issuer.";
    return std::string();
  }

  scoped_refptr<net::X509Certificate> issuer =
      net::x509_util::CreateX509CertificateFromCERTCertificate(
          issuer_handle.get());
  if (!issuer.get()) {
    LOG(ERROR) << "Couldn't create issuer cert.";
    return std::string();
  }
  std::string pem_encoded_issuer;
  if (!net::X509Certificate::GetPEMEncoded(issuer->cert_buffer(),
                                           &pem_encoded_issuer)) {
    LOG(ERROR) << "Couldn't PEM-encode certificate.";
    return std::string();
  }
  return pem_encoded_issuer;
}

void CreateSortedCertAndIssuerList(
    const NetworkCertLoader::NetworkCertList& network_certs,
    base::Time now,
    std::vector<CertAndIssuer>* all_cert_and_issuers,
    std::vector<CertAndIssuer>* device_wide_cert_and_issuers) {
  // Filter all client certs and determines each certificate's issuer, which is
  // required for the pattern matching.
  for (const NetworkCertLoader::NetworkCert& network_cert : network_certs) {
    // If the caller is interested in device-wide certificates only, skip
    // user-specific certificates.
    if (!all_cert_and_issuers && !network_cert.is_device_wide())
      continue;

    CERTCertificate* cert = network_cert.cert();
    base::Time not_after;
    if (!net::x509_util::GetValidityTimes(cert, nullptr, &not_after) ||
        now > not_after ||
        !NetworkCertLoader::IsCertificateHardwareBacked(cert)) {
      continue;
    }
    // GetPrivateKeyNickname should be invoked after the checks above for
    // performance reasons.
    base::Optional<std::string> private_key_nickname =
        GetPrivateKeyNickname(cert);
    if (!private_key_nickname.has_value()) {
      // No private key has been found for this certificate.
      continue;
    }
    std::string pem_encoded_issuer = GetPEMEncodedIssuer(cert);
    if (all_cert_and_issuers) {
      all_cert_and_issuers->push_back(
          CertAndIssuer(net::x509_util::DupCERTCertificate(cert),
                        pem_encoded_issuer, private_key_nickname.value()));
    }
    if (device_wide_cert_and_issuers && network_cert.is_device_wide()) {
      device_wide_cert_and_issuers->push_back(
          CertAndIssuer(net::x509_util::DupCERTCertificate(cert),
                        pem_encoded_issuer, private_key_nickname.value()));
    }
  }

  if (all_cert_and_issuers) {
    std::sort(all_cert_and_issuers->begin(), all_cert_and_issuers->end(),
              &CompareCertExpiration);
  }
  if (device_wide_cert_and_issuers) {
    std::sort(device_wide_cert_and_issuers->begin(),
              device_wide_cert_and_issuers->end(), &CompareCertExpiration);
  }
}

// Searches for matches between |networks| and |network_certs|. Returns the
// matches that were found. Because this calls NSS functions and is potentially
// slow, it must be run on a worker thread.
std::vector<NetworkAndMatchingCert> FindCertificateMatches(
    NetworkCertLoader::NetworkCertList network_certs,
    const std::vector<NetworkAndCertConfig>& networks,
    base::Time now) {
  std::vector<NetworkAndMatchingCert> matches;

  std::vector<CertAndIssuer> all_client_cert_and_issuers;
  std::vector<CertAndIssuer> device_wide_client_cert_and_issuers;
  CreateSortedCertAndIssuerList(network_certs, now,
                                &all_client_cert_and_issuers,
                                &device_wide_client_cert_and_issuers);

  for (const NetworkAndCertConfig& network_and_cert_config : networks) {
    // Use only certs from the system token if the source of the client cert
    // pattern is device policy.
    std::vector<CertAndIssuer>* client_certs =
        network_and_cert_config.cert_config.onc_source ==
                ::onc::ONC_SOURCE_DEVICE_POLICY
            ? &device_wide_client_cert_and_issuers
            : &all_client_cert_and_issuers;
    auto cert_it = std::find_if(
        client_certs->begin(), client_certs->end(),
        MatchCertWithCertConfig(network_and_cert_config.cert_config));
    if (cert_it == client_certs->end()) {
      VLOG(1) << "Couldn't find a matching client cert for network "
              << network_and_cert_config.service_path;
      matches.push_back(
          NetworkAndMatchingCert(network_and_cert_config, base::nullopt));
      continue;
    }

    std::string pkcs11_id;
    int slot_id = -1;

    pkcs11_id = NetworkCertLoader::GetPkcs11IdAndSlotForCert(
        cert_it->cert.get(), &slot_id);
    if (pkcs11_id.empty()) {
      LOG(ERROR) << "Couldn't determine PKCS#11 ID.";
      // So far this error is not expected to happen. We can just continue, in
      // the worst case the user can remove the problematic cert.
      continue;
    }

    // Expand placeholders in the identity string that are specific to the
    // client certificate.
    VariableExpander variable_expander(
        GetSubstitutionsForCert(cert_it->cert.get()));
    std::string identity = network_and_cert_config.cert_config.policy_identity;
    const bool success = variable_expander.ExpandString(&identity);
    LOG_IF(ERROR, !success)
        << "Error during variable expansion in ONC-configured identity";

    matches.push_back(NetworkAndMatchingCert(
        network_and_cert_config, MatchingCert(pkcs11_id, slot_id, identity)));
  }
  return matches;
}

void LogError(const std::string& service_path,
              const std::string& dbus_error_name,
              const std::string& dbus_error_message) {
  network_handler::ShillErrorCallbackFunction(
      "ClientCertResolver.SetProperties failed",
      service_path,
      network_handler::ErrorCallback(),
      dbus_error_name,
      dbus_error_message);
}

bool ClientCertificatesLoaded() {
  if (!NetworkCertLoader::Get()->initial_load_finished()) {
    VLOG(1) << "Certificates not loaded yet.";
    return false;
  }
  return true;
}

}  // namespace

ClientCertResolver::ClientCertResolver()
    : resolve_task_running_(false),
      network_properties_changed_(false),
      network_state_handler_(nullptr),
      managed_network_config_handler_(nullptr),
      testing_clock_(nullptr) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

ClientCertResolver::~ClientCertResolver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (network_state_handler_)
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  if (NetworkCertLoader::IsInitialized())
    NetworkCertLoader::Get()->RemoveObserver(this);
  if (managed_network_config_handler_)
    managed_network_config_handler_->RemoveObserver(this);
}

void ClientCertResolver::Init(
    NetworkStateHandler* network_state_handler,
    ManagedNetworkConfigurationHandler* managed_network_config_handler) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(network_state_handler);
  network_state_handler_ = network_state_handler;
  network_state_handler_->AddObserver(this, FROM_HERE);

  DCHECK(managed_network_config_handler);
  managed_network_config_handler_ = managed_network_config_handler;
  managed_network_config_handler_->AddObserver(this);

  NetworkCertLoader::Get()->AddObserver(this);
}

void ClientCertResolver::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void ClientCertResolver::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

bool ClientCertResolver::IsAnyResolveTaskRunning() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return resolve_task_running_;
}

// static
bool ClientCertResolver::ResolveClientCertificateSync(
    const client_cert::ConfigType client_cert_type,
    const client_cert::ClientCertConfig& client_cert_config,
    base::DictionaryValue* shill_properties) {
  if (!ShouldResolveCert(client_cert_config))
    return false;

  // Prepare and sort the list of known client certs. Use only certs from the
  // system token if the source of the client cert config is device policy.
  std::vector<CertAndIssuer> client_cert_and_issuers;
  if (client_cert_config.onc_source == ::onc::ONC_SOURCE_DEVICE_POLICY) {
    CreateSortedCertAndIssuerList(
        NetworkCertLoader::Get()->client_certs(), base::Time::Now(),
        nullptr /* all_cert_and_issuers */,
        &client_cert_and_issuers /* device_wide_cert_and_issuers */);
  } else {
    CreateSortedCertAndIssuerList(
        NetworkCertLoader::Get()->client_certs(), base::Time::Now(),
        &client_cert_and_issuers /* all_cert_and_issuers */,
        nullptr /* device_wide_cert_and_issuers */);
  }

  // Search for a certificate matching the pattern or reference.
  std::vector<CertAndIssuer>::iterator cert_it = std::find_if(
      client_cert_and_issuers.begin(), client_cert_and_issuers.end(),
      MatchCertWithCertConfig(client_cert_config));

  if (cert_it == client_cert_and_issuers.end()) {
    VLOG(1) << "Couldn't find a matching client cert";
    client_cert::SetEmptyShillProperties(client_cert_type, shill_properties);
    return false;
  }

  int slot_id = -1;
  std::string pkcs11_id = NetworkCertLoader::GetPkcs11IdAndSlotForCert(
      cert_it->cert.get(), &slot_id);
  if (pkcs11_id.empty()) {
    LOG(ERROR) << "Couldn't determine PKCS#11 ID.";
    // So far this error is not expected to happen. We can just continue, in
    // the worst case the user can remove the problematic cert.
    return false;
  }
  client_cert::SetShillProperties(
      client_cert_type, slot_id, pkcs11_id, shill_properties);
  return true;
}

void ClientCertResolver::SetClockForTesting(base::Clock* clock) {
  testing_clock_ = clock;
}

void ClientCertResolver::NetworkListChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "NetworkListChanged.";
  if (!ClientCertificatesLoaded())
    return;
  // Configure only networks that were not configured before.

  // We'll drop networks from |networks_status_|, which are not known anymore.
  base::flat_map<std::string, MatchingCertAndResolveStatus> old_networks_status;
  old_networks_status.swap(networks_status_);

  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Default(),
      true /* configured_only */,
      false /* visible_only */,
      0 /* no limit */,
      &networks);

  NetworkStateHandler::NetworkStateList networks_to_check;
  for (const NetworkState* network : networks) {
    const std::string& service_path = network->path();
    auto old_networks_status_iter = old_networks_status.find(service_path);
    if (old_networks_status_iter != old_networks_status.end()) {
      networks_status_[service_path] = old_networks_status_iter->second;
      continue;
    }
    networks_to_check.push_back(network);
  }

  if (!networks_to_check.empty()) {
    NET_LOG(EVENT) << "ClientCertResolver: NetworkListChanged: "
                   << networks_to_check.size();
    ResolveNetworks(networks_to_check);
  }
}

void ClientCertResolver::NetworkConnectionStateChanged(
    const NetworkState* network) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!ClientCertificatesLoaded())
    return;
  if (!network->IsConnectingOrConnected()) {
    NET_LOG(EVENT) << "ClientCertResolver: ConnectionStateChanged: "
                   << network->name();
    ResolveNetworks(NetworkStateHandler::NetworkStateList(1, network));
  }
}

void ClientCertResolver::OnCertificatesLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "OnCertificatesLoaded.";
  if (!ClientCertificatesLoaded())
    return;
  NET_LOG(EVENT) << "ClientCertResolver: Certificates Loaded.";
  // Compare all networks with all certificates.
  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Default(),
      true /* configured_only */,
      false /* visible_only */,
      0 /* no limit */,
      &networks);
  ResolveNetworks(networks);
}

void ClientCertResolver::PolicyAppliedToNetwork(
    const std::string& service_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "PolicyAppliedToNetwork " << service_path;
  if (!ClientCertificatesLoaded())
    return;
  // Compare this network with all certificates.
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromServicePath(
          service_path, true /* configured_only */);
  if (!network) {
    LOG(ERROR) << "service path '" << service_path << "' unknown.";
    return;
  }
  NET_LOG(EVENT) << "ClientCertResolver: PolicyAppliedToNetwork: "
                 << network->name();
  NetworkStateHandler::NetworkStateList networks;
  networks.push_back(network);
  ResolveNetworks(networks);
}

void ClientCertResolver::ResolveNetworks(
    const NetworkStateHandler::NetworkStateList& networks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<NetworkAndCertConfig> networks_to_resolve;

  // Filter networks with ClientCertPattern or ClientCertRef. As these can only
  // be set by policy, we check there.
  for (const NetworkState* network : networks) {
    // If the network was not known before, mark it as known but with resolving
    // pending.
    if (networks_status_.find(network->path()) == networks_status_.end())
      networks_status_.insert_or_assign(network->path(),
                                        MatchingCertAndResolveStatus());

    // If this network is not configured, it cannot have a
    // ClientCertPattern/ClientCertRef.
    if (network->profile_path().empty())
      continue;

    ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_NONE;
    const base::DictionaryValue* policy =
        managed_network_config_handler_->FindPolicyByGuidAndProfile(
            network->guid(), network->profile_path(), &onc_source);

    if (!policy) {
      VLOG(1) << "The policy for network " << network->path() << " with GUID "
              << network->guid() << " is not available yet.";
      // Skip this network for now. Once the policy is loaded, PolicyApplied()
      // will retry.
      continue;
    }

    VLOG(2) << "Inspecting network " << network->path();
    client_cert::ClientCertConfig cert_config;
    OncToClientCertConfig(onc_source, *policy, &cert_config);

    // Skip networks that don't have a ClientCertPattern or ClientCertRef.
    if (!ShouldResolveCert(cert_config))
      continue;

    networks_to_resolve.push_back(
        NetworkAndCertConfig(network->path(), cert_config));
  }

  if (networks_to_resolve.empty()) {
    VLOG(1) << "No networks to resolve.";
    // If a resolve task is running, it will notify observers when it's
    // finished.
    if (!resolve_task_running_)
      NotifyResolveRequestCompleted();
    return;
  }

  if (resolve_task_running_) {
    VLOG(1) << "A resolve task is already running. Queue this request.";
    for (const NetworkAndCertConfig& network_to_resolve : networks_to_resolve)
      queued_networks_to_resolve_.insert(network_to_resolve.service_path);
    return;
  }

  VLOG(2) << "Start task for resolving client cert patterns.";
  resolve_task_running_ = true;
  base::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&FindCertificateMatches,
                     NetworkCertLoader::CloneNetworkCertList(
                         NetworkCertLoader::Get()->client_certs()),
                     networks_to_resolve, Now()),
      base::BindOnce(&ClientCertResolver::ConfigureCertificates,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClientCertResolver::ResolvePendingNetworks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(NetworkTypePattern::Default(),
                                               true /* configured_only */,
                                               false /* visible_only */,
                                               0 /* no limit */,
                                               &networks);

  NetworkStateHandler::NetworkStateList networks_to_resolve;
  for (const NetworkState* network : networks) {
    if (queued_networks_to_resolve_.count(network->path()) > 0)
      networks_to_resolve.push_back(network);
  }
  VLOG(1) << "Resolve pending " << networks_to_resolve.size() << " networks.";
  queued_networks_to_resolve_.clear();
  ResolveNetworks(networks_to_resolve);
}

void ClientCertResolver::ConfigureCertificates(
    std::vector<NetworkAndMatchingCert> matches) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const NetworkAndMatchingCert& match : matches) {
    MatchingCertAndResolveStatus& network_status =
        networks_status_[match.service_path];
    if (network_status.resolve_status == ResolveStatus::kResolved &&
        network_status.matching_cert == match.matching_cert) {
      // The same certificate was configured in the last ConfigureCertificates
      // call, so don't do anything for this network.
      continue;
    }
    network_status.resolve_status = ResolveStatus::kResolved;
    network_status.matching_cert = match.matching_cert;
    network_properties_changed_ = true;

    NET_LOG(EVENT) << "Configuring certificate for network: "
                   << match.service_path;

    base::DictionaryValue shill_properties;
    if (match.matching_cert.has_value()) {
      const MatchingCert& matching_cert = match.matching_cert.value();
      client_cert::SetShillProperties(
          match.cert_config_type, matching_cert.key_slot_id,
          matching_cert.pkcs11_id, &shill_properties);
      if (!matching_cert.identity.empty()) {
        shill_properties.SetKey(shill::kEapIdentityProperty,
                                base::Value(matching_cert.identity));
      }
    } else {
      client_cert::SetEmptyShillProperties(match.cert_config_type,
                                           &shill_properties);
    }
    ShillServiceClient::Get()->SetProperties(
        dbus::ObjectPath(match.service_path), shill_properties,
        base::DoNothing(), base::BindRepeating(&LogError, match.service_path));
    network_state_handler_->RequestUpdateForNetwork(match.service_path);
  }
  resolve_task_running_ = false;
  if (queued_networks_to_resolve_.empty())
    NotifyResolveRequestCompleted();
  else
    ResolvePendingNetworks();
}

void ClientCertResolver::NotifyResolveRequestCompleted() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!resolve_task_running_);

  VLOG(2) << "Notify observers: " << (network_properties_changed_ ? "" : "no ")
          << "networks changed.";
  const bool changed = network_properties_changed_;
  network_properties_changed_ = false;
  for (auto& observer : observers_)
    observer.ResolveRequestCompleted(changed);
}

base::Time ClientCertResolver::Now() const {
  if (testing_clock_)
    return testing_clock_->Now();
  return base::Time::Now();
}

}  // namespace chromeos
