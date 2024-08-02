// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/ash/components/network/client_cert_resolver.h"

#include <cert.h>
#include <certt.h>  // for (SECCertUsageEnum) certUsageAnyCA
#include <pk11pub.h>

#include <memory>
#include <optional>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/task/thread_pool.h"
#include "base/time/clock.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/certificate_helper.h"
#include "chromeos/ash/components/network/client_cert_util.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_event_log.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/onc/onc_certificate_pattern.h"
#include "chromeos/components/onc/variable_expander.h"
#include "components/onc/onc_constants.h"
#include "crypto/scoped_nss_types.h"
#include "dbus/object_path.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"
#include "third_party/cros_system_api/constants/pkcs11_custom_attributes.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

using ResolvedCert = client_cert::ResolvedCert;

std::string GetNetworkIdWithGuid(const NetworkState* network_state) {
  return NetworkId(network_state) + "[guid='" + network_state->guid() + "']";
}

// Global override for the getter function. This is used for testing purposes.
// See SetProvisioningIdForCertGetterForTesting for details.
ClientCertResolver::ProvisioningProfileIdGetter
    g_provisioning_id_getter_for_testing =
        ClientCertResolver::ProvisioningProfileIdGetter();

// Describes a network that is configured with |client_cert_config|, which
// includes the certificate config.
struct NetworkAndCertConfig {
  NetworkAndCertConfig(const std::string& network_path,
                       const std::string& userhash,
                       const std::string& guid,
                       const client_cert::ClientCertConfig& client_cert_config)
      : service_path(network_path),
        userhash(userhash),
        guid(guid),
        cert_config(client_cert_config) {}

  std::string service_path;
  std::string userhash;
  std::string guid;

  client_cert::ClientCertConfig cert_config;
};

// Returns substitutions based on |cert|'s contents to be used in a
// VariableExpander.
base::flat_map<std::string, std::string> GetSubstitutionsForCert(
    CERTCertificate* cert) {
  base::flat_map<std::string, std::string> substitutions;

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

std::optional<ResolvedCert> GetResolvedCert(CERTCertificate* cert) {
  int slot_id = -1;
  std::string pkcs11_id =
      NetworkCertLoader::GetPkcs11IdAndSlotForCert(cert, &slot_id);
  if (pkcs11_id.empty()) {
    LOG(ERROR) << "Can't extract PKCS11 id";
    return {};
  }
  return ResolvedCert::CertMatched(slot_id, pkcs11_id,
                                   GetSubstitutionsForCert(cert));
}

// Returns true if |client_cert_config| specifies a pattern or reference, i.e.
// if client certificate resolution should be attempted.
bool ShouldResolveCert(
    const client_cert::ClientCertConfig& client_cert_config) {
  return client_cert_config.client_cert_type == ::onc::client_cert::kPattern ||
         client_cert_config.client_cert_type == ::onc::client_cert::kRef ||
         client_cert_config.client_cert_type ==
             ::onc::client_cert::kProvisioningProfileId;
}

}  // namespace

namespace internal {

// Describes a network and the client cert resolution result for that network.
struct NetworkAndMatchingCert {
  NetworkAndMatchingCert(const NetworkAndCertConfig& network_and_cert_config,
                         ResolvedCert resolved_cert)
      : service_path(network_and_cert_config.service_path),
        userhash(network_and_cert_config.userhash),
        guid(network_and_cert_config.guid),
        resolved_cert(std::move(resolved_cert)) {}

  std::string service_path;
  std::string userhash;
  std::string guid;

  // The resolved certificate, or |ResolvedCert::NothingMatched()| if no
  // matching certificate has been found.
  ResolvedCert resolved_cert;
};

}  // namespace internal

using internal::NetworkAndMatchingCert;

namespace {

// Returns the nickname of the private key for certificate |cert|, if such a
// private key is installed. Note that this is not a cheap operation: it
// iterates all tokens and attempts to look up the private key.
// A return value of |std::nullopt| means that no private key could be found
// for |cert|.
// If a private key could be found for |cert| but it did not have a nickname,
// will return the empty string.
std::optional<std::string> GetPrivateKeyNickname(CERTCertificate* cert) {
  crypto::ScopedSECKEYPrivateKey key(
      PK11_FindKeyByAnyCert(cert, /*wincx=*/nullptr));
  if (!key)
    return std::nullopt;

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
                const std::string& key_nickname,
                const std::string& cert_provisioning_profile_id)
      : cert(std::move(certificate)),
        pem_encoded_issuer(issuer),
        private_key_nickname(key_nickname),
        provisioning_profile_id(cert_provisioning_profile_id) {}

  net::ScopedCERTCertificate cert;
  std::string pem_encoded_issuer;
  // The nickname of the private key associated with |cert|. This is used
  // for resolving ClientCertRef ONC references, using the fact that
  // |CertificateImporterImpl| sets the private key nickname to the
  // ONC-specified GUID.
  std::string private_key_nickname;
  std::string provisioning_profile_id;
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
      if (cert_config.guid == cert_and_issuer.private_key_nickname)
        return true;
      // This code implements a method used by Android which also checks
      // if the ref matches the provisioning_profile_id, if kRef was chosen.
      // Since ref and provisioning_profile_id come from different namespaces
      // there should not be a collision. The more proper version is below.
      return cert_config.guid == cert_and_issuer.provisioning_profile_id;
    }

    // This is the proper configuration in the ONC file for using a
    // provisioning profile id for selecting the certificate.
    if (cert_config.client_cert_type ==
        ::onc::client_cert::kProvisioningProfileId) {
      return cert_config.provisioning_profile_id ==
             cert_and_issuer.provisioning_profile_id;
    }

    NOTREACHED_IN_MIGRATION();
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

std::string GetProvisioningIdForCert(CERTCertificate* cert) {
  crypto::ScopedSECKEYPrivateKey priv_key(
      PK11_FindKeyByAnyCert(cert, /*wincx=*/nullptr));
  if (!priv_key)
    return std::string();

  if (!g_provisioning_id_getter_for_testing.is_null())
    return g_provisioning_id_getter_for_testing.Run(cert);

  crypto::ScopedSECItem attribute_value(SECITEM_AllocItem(/*arena=*/nullptr,
                                                          /*item=*/nullptr,
                                                          /*len=*/0));
  DCHECK(attribute_value.get());

  SECStatus status = PK11_ReadRawAttribute(
      /*objType=*/PK11_TypePrivKey, priv_key.get(),
      pkcs11_custom_attributes::kCkaChromeOsBuiltinProvisioningProfileId,
      attribute_value.get());
  if (status != SECSuccess) {
    return std::string();
  }

  if (attribute_value->len > 0) {
    std::string id;
    id.assign(attribute_value->data,
              attribute_value->data + attribute_value->len);
    return id;
  }

  return std::string();
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

    if (!network_cert.is_available_for_network_auth())
      continue;

    CERTCertificate* cert = network_cert.cert();
    base::Time not_after;
    if (!net::x509_util::GetValidityTimes(cert, nullptr, &not_after) ||
        now > not_after) {
      continue;
    }
    // GetPrivateKeyNickname should be invoked after the checks above for
    // performance reasons.
    std::optional<std::string> private_key_nickname =
        GetPrivateKeyNickname(cert);
    if (!private_key_nickname.has_value()) {
      // No private key has been found for this certificate.
      continue;
    }

    std::string cert_id = GetProvisioningIdForCert(cert);

    std::string pem_encoded_issuer = GetPEMEncodedIssuer(cert);
    if (all_cert_and_issuers) {
      all_cert_and_issuers->push_back(CertAndIssuer(
          net::x509_util::DupCERTCertificate(cert), pem_encoded_issuer,
          private_key_nickname.value(), cert_id));
    }
    if (device_wide_cert_and_issuers && network_cert.is_device_wide()) {
      device_wide_cert_and_issuers->push_back(CertAndIssuer(
          net::x509_util::DupCERTCertificate(cert), pem_encoded_issuer,
          private_key_nickname.value(), cert_id));
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
    auto cert_it = base::ranges::find_if(
        *client_certs,
        MatchCertWithCertConfig(network_and_cert_config.cert_config));
    if (cert_it == client_certs->end()) {
      VLOG(1) << "Couldn't find a matching client cert for network "
              << network_and_cert_config.service_path;
      matches.push_back(
          NetworkAndMatchingCert(network_and_cert_config,
                                 client_cert::ResolvedCert::NothingMatched()));
      continue;
    }

    std::optional<ResolvedCert> resolved_cert =
        GetResolvedCert(cert_it->cert.get());
    if (!resolved_cert) {
      LOG(ERROR) << "Couldn't determine PKCS#11 ID.";
      // So far this error is not expected to happen. We can just continue, in
      // the worst case the user can remove the problematic cert.
      continue;
    }

    matches.push_back(NetworkAndMatchingCert(network_and_cert_config,
                                             std::move(resolved_cert.value())));
  }
  return matches;
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
  network_state_handler_observer_.Observe(network_state_handler_.get());

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
    base::Value::Dict* shill_properties) {
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

  // Search for a certificate matching the pattern, reference or
  // ProvisioningProfileId.
  std::vector<CertAndIssuer>::iterator cert_it = base::ranges::find_if(
      client_cert_and_issuers, MatchCertWithCertConfig(client_cert_config));

  if (cert_it == client_cert_and_issuers.end()) {
    VLOG(1) << "Couldn't find a matching client cert";
    client_cert::SetEmptyShillProperties(client_cert_type, *shill_properties);
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
  client_cert::SetShillProperties(client_cert_type, slot_id, pkcs11_id,
                                  *shill_properties);
  return true;
}

void ClientCertResolver::SetClockForTesting(base::Clock* clock) {
  testing_clock_ = clock;
}

// static
base::ScopedClosureRunner
ClientCertResolver::SetProvisioningIdForCertGetterForTesting(
    ProvisioningProfileIdGetter getter) {
  g_provisioning_id_getter_for_testing = getter;

  return base::ScopedClosureRunner(
      base::BindOnce([]() { g_provisioning_id_getter_for_testing.Reset(); }));
}

void ClientCertResolver::NetworkListChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  VLOG(2) << "NetworkListChanged.";
  if (!ClientCertificatesLoaded())
    return;
  // Configure only networks that were not configured before.

  // We'll drop networks from |known_networks_service_paths_| which are not
  // known anymore.
  base::flat_set<std::string> old_known_networks_service_paths;
  old_known_networks_service_paths.swap(known_networks_service_paths_);

  NetworkStateHandler::NetworkStateList networks;
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Default(), true /* configured_only */,
      false /* visible_only */, 0 /* no limit */, &networks);

  NetworkStateHandler::NetworkStateList networks_to_check;
  for (const NetworkState* network : networks) {
    const std::string& service_path = network->path();
    known_networks_service_paths_.insert(service_path);
    if (!old_known_networks_service_paths.contains(service_path)) {
      networks_to_check.push_back(network);
    }
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
                   << GetNetworkIdWithGuid(network);
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
      NetworkTypePattern::Default(), true /* configured_only */,
      false /* visible_only */, 0 /* no limit */, &networks);
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
                 << GetNetworkIdWithGuid(network);

  NetworkStateHandler::NetworkStateList networks;
  networks.push_back(network);
  ResolveNetworks(networks);
}

void ClientCertResolver::ResolveNetworks(
    const NetworkStateHandler::NetworkStateList& networks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<NetworkAndCertConfig> networks_to_resolve;
  std::vector<std::string> networks_to_resolve_info;

  // Filter networks with ClientCertPattern, ClientCertRef, or
  // ProvisioningProfileId. As these can only be set by policy, we check there.
  for (const NetworkState* network : networks) {
    // If this network is not configured, it cannot have a
    // ClientCertPattern/ClientCertRef/ProvisioningProfileId.
    if (network->profile_path().empty())
      continue;

    known_networks_service_paths_.insert(network->path());

    ::onc::ONCSource onc_source = ::onc::ONC_SOURCE_NONE;
    std::string userhash;
    const base::Value::Dict* policy =
        managed_network_config_handler_->FindPolicyByGuidAndProfile(
            network->guid(), network->profile_path(),
            ManagedNetworkConfigurationHandler::PolicyType::kOriginal,
            &onc_source, &userhash);

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

    networks_to_resolve.push_back(NetworkAndCertConfig(
        network->path(), userhash, network->guid(), cert_config));
    networks_to_resolve_info.push_back(GetNetworkIdWithGuid(network));
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

  NET_LOG(EVENT) << "Start task for resolving client cert patterns for "
                 << base::JoinString(networks_to_resolve_info, ", ");
  resolve_task_running_ = true;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
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
  network_state_handler_->GetNetworkListByType(
      NetworkTypePattern::Default(), true /* configured_only */,
      false /* visible_only */, 0 /* no limit */, &networks);

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
  std::vector<std::string> resolved_networks_info;
  for (NetworkAndMatchingCert& match : matches) {
    const NetworkState* network_state =
        network_state_handler_->GetNetworkStateFromServicePath(
            match.service_path, /*configured_only=*/true);
    if (!network_state) {
      resolved_networks_info.push_back(match.service_path +
                                       ":skipped(not configured)");
      continue;
    }

    resolved_networks_info.push_back(
        GetNetworkIdWithGuid(network_state) +
        (match.resolved_cert.status() == ResolvedCert::Status::kCertMatched
             ? ":match"
             : ":no_match"));
    network_properties_changed_ |=
        managed_network_config_handler_->SetResolvedClientCertificate(
            match.userhash, match.guid, std::move(match.resolved_cert));
  }
  NET_LOG(EVENT) << "Summary: "
                 << base::JoinString(resolved_networks_info, ", ");

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

}  // namespace ash
