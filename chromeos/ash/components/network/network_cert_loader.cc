// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_cert_loader.h"

#include <algorithm>
#include <initializer_list>
#include <map>
#include <memory>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/network/certificate_helper.h"
#include "chromeos/ash/components/network/policy_certificate_provider.h"
#include "chromeos/ash/components/network/system_token_cert_db_storage.h"
#include "chromeos/components/onc/certificate_scope.h"
#include "crypto/chaps_support.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_util_nss.h"

namespace ash {

namespace {

bool g_force_available_for_network_auth_for_test = false;
NetworkCertLoader* g_cert_loader = nullptr;

enum class NetworkCertType {
  kAuthorityCertificate,
  kClientCertificate,
  kOther
};

NetworkCertType GetNetworkCertType(CERTCertificate* cert) {
  net::CertType type = certificate::GetCertType(cert);
  if (type == net::USER_CERT)
    return NetworkCertType::kClientCertificate;
  if (type == net::CA_CERT)
    return NetworkCertType::kAuthorityCertificate;
  VLOG(2) << "Ignoring cert type: " << type;
  return NetworkCertType::kOther;
}

bool IsAvailableForNetworkAuth(CERTCertificate* cert) {
  if (g_force_available_for_network_auth_for_test)
    return true;
  return crypto::IsSlotProvidedByChaps(cert->slot);
}

// Returns all authority certificates with default (not restricted) scope
// provided by |policy_certificate_provider| as a list of NetworkCerts.
NetworkCertLoader::NetworkCertList GetPolicyProvidedAuthorities(
    const PolicyCertificateProvider* policy_certificate_provider,
    bool device_wide) {
  NetworkCertLoader::NetworkCertList result;
  if (!policy_certificate_provider)
    return result;
  for (const auto& certificate :
       policy_certificate_provider->GetAllAuthorityCertificates(
           chromeos::onc::CertificateScope::Default())) {
    net::ScopedCERTCertificate x509_cert =
        net::x509_util::CreateCERTCertificateFromX509Certificate(
            certificate.get());
    if (!x509_cert) {
      LOG(ERROR) << "Unable to create CERTCertificate";
      continue;
    }
    result.push_back(NetworkCertLoader::NetworkCert(
        std::move(x509_cert), /*available_for_network_auth=*/false,
        device_wide));
  }
  return result;
}

// Combines all NetworkCerts from all |network_cert_lists| to a resulting list,
// avoiding duplicates.
NetworkCertLoader::NetworkCertList CombineNetworkCertLists(
    std::initializer_list<const NetworkCertLoader::NetworkCertList*>
        network_cert_lists) {
  size_t total_size = 0;
  for (const NetworkCertLoader::NetworkCertList* list : network_cert_lists)
    total_size += list->size();
  NetworkCertLoader::NetworkCertList result;
  result.reserve(total_size);

  std::map<const CERTCertificate*, size_t> added_cert_to_position;
  for (const NetworkCertLoader::NetworkCertList* list : network_cert_lists) {
    for (const NetworkCertLoader::NetworkCert& network_cert : *list) {
      auto it = added_cert_to_position.find(network_cert.cert());
      if (it == added_cert_to_position.end()) {
        // This certificate wasn't added before.
        // Add it and save its position in the result list.
        added_cert_to_position.insert({network_cert.cert(), result.size()});
        result.push_back(network_cert.Clone());
      } else if (network_cert.is_device_wide()) {
        // Replace the already added certificate with the device-wide one so
        // that it can be used for shared configurations.
        size_t position = it->second;
        result[position] = network_cert.Clone();
      }
    }
  }
  return result;
}

}  // namespace

// Caches certificates from a single slot of a NSSCertDatabase. Handles
// reloading of certificates on update notifications and provides status flags
// (loading / loaded). NetworkCertLoader can use multiple CertCaches to combine
// certificates from multiple sources.
class NetworkCertLoader::CertCache : public net::CertDatabase::Observer {
 public:
  enum class State {
    // The CertCache is not initialized and not expected to be initialized soon.
    kNotInitialized,
    // The CertCache is expected to be initialized soon.
    kMarkedWillBeInitialized,
    // The CertCache initialization has started, the initial load of
    // certificates is in progress.
    kInitialLoadInProgress,
    // The CertCache is initialized and currently not re-loading certificates.
    kInitializedAndIdle,
    // The CertCache is initialized and currently re-loading certificates.
    kInitializedAndReloading
  };

  explicit CertCache(base::RepeatingClosure certificates_updated_callback)
      : certificates_updated_callback_(certificates_updated_callback) {}

  CertCache(const CertCache&) = delete;
  CertCache& operator=(const CertCache&) = delete;

  ~CertCache() override {
    net::CertDatabase::GetInstance()->RemoveObserver(this);
  }

  void MarkWillBeInitialized(bool will_be_initialized) {
    DCHECK(state_ == State::kNotInitialized ||
           state_ == State::kMarkedWillBeInitialized);
    state_ = will_be_initialized ? State::kMarkedWillBeInitialized
                                 : State::kNotInitialized;
  }

  void SetNSSDBAndSlot(net::NSSCertDatabase* nss_database,
                       crypto::ScopedPK11Slot slot,
                       bool is_slot_device_wide) {
    CHECK(!nss_database_);
    CHECK(slot);
    nss_database_ = nss_database;
    slot_ = std::move(slot);
    is_slot_device_wide_ = is_slot_device_wide;

    // Start observing cert database for changes.
    // Observing net::CertDatabase is preferred over observing |nss_database_|
    // directly, as |nss_database_| observers receive only events generated
    // directly by |nss_database_|, so they may miss a few relevant ones.
    // TODO(tbarzic): Once singleton NSSCertDatabase is removed, investigate if
    // it would be OK to observe |nss_database_| directly; or change
    // NSSCertDatabase to send notification on all relevant changes.
    net::CertDatabase::GetInstance()->AddObserver(this);

    LoadCertificates(/*initial_load=*/true);
  }

  net::NSSCertDatabase* nss_database() { return nss_database_; }

  // net::CertDatabase::Observer
  void OnTrustStoreChanged() override {
    VLOG(1) << "OnTrustStoreChanged";
    LoadCertificates(/*initial_load=*/false);
  }
  void OnClientCertStoreChanged() override {
    VLOG(1) << "OnClientCertStoreChanged";
    LoadCertificates(/*initial_load=*/false);
  }

  const NetworkCertList& authority_certs() const { return authority_certs_; }

  const NetworkCertList& client_certs() const { return client_certs_; }

  bool is_or_will_be_initialized() const {
    return state_ != State::kNotInitialized;
  }

  bool is_initialized() const { return nss_database_; }

  bool initial_load_running() const {
    return state_ == State::kInitialLoadInProgress;
  }

  bool certificates_update_running() const {
    return state_ == State::kInitialLoadInProgress ||
           state_ == State::kInitializedAndReloading;
  }

  bool initial_load_finished() const {
    return state_ == State::kInitializedAndIdle ||
           state_ == State::kInitializedAndReloading;
  }

 private:
  // Trigger a certificate load. If a certificate loading task is already in
  // progress, will start a reload once the current task is finished.
  void LoadCertificates(bool initial_load) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    VLOG(1) << "LoadCertificates: " << certificates_update_running();

    if (!nss_database_)
      return;

    if (certificates_update_running()) {
      certificates_update_required_ = true;
      return;
    }

    state_ = initial_load ? State::kInitialLoadInProgress
                          : State::kInitializedAndReloading;
    certificates_update_required_ = false;

    nss_database_->ListCertsInSlot(
        base::BindOnce(&CertCache::UpdateCertificates,
                       weak_factory_.GetWeakPtr()),
        slot_.get());
  }

  // Called if a certificate load task is finished.
  void UpdateCertificates(net::ScopedCERTCertificateList cert_list) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(certificates_update_running());
    VLOG(1) << "UpdateCertificates: " << cert_list.size();

    authority_certs_.clear();
    client_certs_.clear();
    for (auto& cert : cert_list) {
      NetworkCertType type = GetNetworkCertType(cert.get());
      if (type == NetworkCertType::kAuthorityCertificate) {
        authority_certs_.push_back(
            NetworkCert(std::move(cert), /*available_for_network_auth=*/false,
                        is_slot_device_wide_));
      } else if (type == NetworkCertType::kClientCertificate) {
        bool available_for_network_auth = IsAvailableForNetworkAuth(cert.get());
        client_certs_.push_back(NetworkCert(
            std::move(cert), available_for_network_auth, is_slot_device_wide_));
      }
    }

    state_ = State::kInitializedAndIdle;
    certificates_updated_callback_.Run();

    if (certificates_update_required_)
      LoadCertificates(/*initial_load=*/false);
  }

  // To be called when certificates have been updated.
  base::RepeatingClosure certificates_updated_callback_;

  // The state of this CertCache.
  State state_ = State::kNotInitialized;

  // This is true if a notification about certificate DB changes arrived while
  // loading certificates and means that we will have to trigger another
  // certificates load after that.
  bool certificates_update_required_ = false;

  // The NSS certificate database from which the certificates should be loaded.
  // Dangling during LoginIntegrationTest.TestLogin on
  // chromeos-amd64-generic-rel-gtest.
  raw_ptr<net::NSSCertDatabase, AcrossTasksDanglingUntriaged> nss_database_ =
      nullptr;

  // The slot from which certificates are listed.
  crypto::ScopedPK11Slot slot_;

  // true if |slot_| is available device-wide, so certificates listed from it
  // can be used for shared networks.
  bool is_slot_device_wide_ = false;

  // Authority Certificates loaded from the database.
  NetworkCertList authority_certs_;

  // Client Certificates loaded from the database.
  NetworkCertList client_certs_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<CertCache> weak_factory_{this};
};

NetworkCertLoader::NetworkCert::NetworkCert(net::ScopedCERTCertificate cert,
                                            bool available_for_network_auth,
                                            bool device_wide)
    : cert_(std::move(cert)),
      available_for_network_auth_(available_for_network_auth),
      device_wide_(device_wide) {}

NetworkCertLoader::NetworkCert::~NetworkCert() = default;

NetworkCertLoader::NetworkCert::NetworkCert(NetworkCert&& other) = default;

NetworkCertLoader::NetworkCert& NetworkCertLoader::NetworkCert::operator=(
    NetworkCert&& other) = default;

bool NetworkCertLoader::NetworkCert::IsHardwareBacked() const {
  return net::NSSCertDatabase::IsHardwareBacked(cert_.get());
}

NetworkCertLoader::NetworkCert NetworkCertLoader::NetworkCert::Clone() const {
  return NetworkCert(net::x509_util::DupCERTCertificate(cert_.get()),
                     available_for_network_auth_, device_wide_);
}

// static
void NetworkCertLoader::Initialize() {
  CHECK(!g_cert_loader);
  g_cert_loader = new NetworkCertLoader();
}

// static
void NetworkCertLoader::Shutdown() {
  CHECK(g_cert_loader);
  delete g_cert_loader;
  g_cert_loader = nullptr;
}

// static
NetworkCertLoader* NetworkCertLoader::Get() {
  CHECK(g_cert_loader) << "NetworkCertLoader::Get() called before Initialize()";
  return g_cert_loader;
}

// static
bool NetworkCertLoader::IsInitialized() {
  return g_cert_loader;
}

NetworkCertLoader::NetworkCertLoader() {
  system_slot_cert_cache_ = std::make_unique<CertCache>(base::BindRepeating(
      &NetworkCertLoader::OnCertCacheUpdated, base::Unretained(this)));
  user_private_slot_cert_cache_ =
      std::make_unique<CertCache>(base::BindRepeating(
          &NetworkCertLoader::OnCertCacheUpdated, base::Unretained(this)));
  user_public_slot_cert_cache_ =
      std::make_unique<CertCache>(base::BindRepeating(
          &NetworkCertLoader::OnCertCacheUpdated, base::Unretained(this)));

  auto* system_token_cert_db_storage = SystemTokenCertDbStorage::Get();
  DCHECK(system_token_cert_db_storage);

  system_token_cert_db_storage->GetDatabase(base::BindOnce(
      &NetworkCertLoader::OnSystemNssDbReady, weak_factory_.GetWeakPtr()));
}

NetworkCertLoader::~NetworkCertLoader() {
  DCHECK(!device_policy_certificate_provider_);
  DCHECK(!user_policy_certificate_provider_);
}

void NetworkCertLoader::MarkSystemNSSDBWillBeInitialized() {
  system_slot_cert_cache_->MarkWillBeInitialized(true);
}

void NetworkCertLoader::SetSystemNssDbForTesting(
    net::NSSCertDatabase* system_slot_database) {
  system_slot_cert_cache_->SetNSSDBAndSlot(
      system_slot_database, system_slot_database->GetSystemSlot(),
      true /* is_slot_device_wide */);
}

void NetworkCertLoader::MarkUserNSSDBWillBeInitialized() {
  user_private_slot_cert_cache_->MarkWillBeInitialized(true);
  user_public_slot_cert_cache_->MarkWillBeInitialized(true);
}

void NetworkCertLoader::SetUserNSSDB(net::NSSCertDatabase* user_database) {
  // The private slot can be absent.
  crypto::ScopedPK11Slot private_slot = user_database->GetPrivateSlot();
  if (private_slot) {
    user_private_slot_cert_cache_->SetNSSDBAndSlot(
        user_database, std::move(private_slot),
        false /* is_slot_device_wide */);
  } else {
    user_private_slot_cert_cache_->MarkWillBeInitialized(false);
  }
  user_public_slot_cert_cache_->SetNSSDBAndSlot(
      user_database, user_database->GetPublicSlot(),
      false /* is_slot_device_wide */);
}

void NetworkCertLoader::SetDevicePolicyCertificateProvider(
    PolicyCertificateProvider* device_policy_certificate_provider) {
  if (device_policy_certificate_provider_) {
    device_policy_certificate_provider_->RemovePolicyProvidedCertsObserver(
        this);
  }
  device_policy_certificate_provider_ = device_policy_certificate_provider;
  if (device_policy_certificate_provider_)
    device_policy_certificate_provider_->AddPolicyProvidedCertsObserver(this);
  UpdateCertificates();
}

void NetworkCertLoader::SetUserPolicyCertificateProvider(
    PolicyCertificateProvider* user_policy_certificate_provider) {
  if (user_policy_certificate_provider_)
    user_policy_certificate_provider_->RemovePolicyProvidedCertsObserver(this);
  user_policy_certificate_provider_ = user_policy_certificate_provider;
  if (user_policy_certificate_provider_)
    user_policy_certificate_provider_->AddPolicyProvidedCertsObserver(this);
  UpdateCertificates();
}

void NetworkCertLoader::AddObserver(NetworkCertLoader::Observer* observer) {
  observers_.AddObserver(observer);
}

void NetworkCertLoader::RemoveObserver(NetworkCertLoader::Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool NetworkCertLoader::initial_load_of_any_database_running() const {
  return system_slot_cert_cache_->initial_load_running() ||
         user_private_slot_cert_cache_->initial_load_running() ||
         user_public_slot_cert_cache_->initial_load_running();
}

bool NetworkCertLoader::initial_load_finished() const {
  return system_slot_cert_cache_->initial_load_finished() ||
         user_cert_database_load_finished();
}

bool NetworkCertLoader::user_cert_database_load_finished() const {
  if (!user_public_slot_cert_cache_->is_initialized())
    return false;

  // The private slot is optional, so it's possible that the private slot cert
  // cache is not initialized. In this case, only care about the public slot
  // cert cache's state.
  if (!user_private_slot_cert_cache_->is_initialized())
    return user_public_slot_cert_cache_->initial_load_finished();

  return user_private_slot_cert_cache_->initial_load_finished() &&
         user_public_slot_cert_cache_->initial_load_finished();
}

bool NetworkCertLoader::can_have_client_certificates() const {
  return system_slot_cert_cache_->is_or_will_be_initialized() ||
         user_private_slot_cert_cache_->is_or_will_be_initialized();
}

// static
net::ScopedCERTCertificateList
NetworkCertLoader::GetAllCertsFromNetworkCertList(
    const NetworkCertList& network_cert_list) {
  net::ScopedCERTCertificateList result;
  result.reserve(network_cert_list.size());
  for (const NetworkCert& network_cert : network_cert_list) {
    result.push_back(net::x509_util::DupCERTCertificate(network_cert.cert()));
  }
  return result;
}

// static
NetworkCertLoader::NetworkCertList NetworkCertLoader::CloneNetworkCertList(
    const NetworkCertList& network_cert_list) {
  NetworkCertList result;
  result.reserve(network_cert_list.size());
  for (const NetworkCert& network_cert : network_cert_list) {
    result.push_back(network_cert.Clone());
  }
  return result;
}

// static
void NetworkCertLoader::ForceAvailableForNetworkAuthForTesting() {
  g_force_available_for_network_auth_for_test = true;
}

// static
//
// For background see this discussion on dev-tech-crypto.lists.mozilla.org:
// http://web.archiveorange.com/archive/v/6JJW7E40sypfZGtbkzxX
//
// NOTE: This function relies on the convention that the same PKCS#11 ID
// is shared between a certificate and its associated private and public
// keys.  I tried to implement this with PK11_GetLowLevelKeyIDForCert(),
// but that always returns NULL on Chrome OS for me.
std::string NetworkCertLoader::GetPkcs11IdAndSlotForCert(CERTCertificate* cert,
                                                         int* slot_id) {
  DCHECK(slot_id);

  SECKEYPrivateKey* priv_key = PK11_FindKeyByAnyCert(cert, nullptr /* wincx */);
  if (!priv_key)
    return std::string();

  *slot_id = static_cast<int>(PK11_GetSlotID(priv_key->pkcs11Slot));

  // Get the CKA_ID attribute for a key.
  SECItem* sec_item = PK11_GetLowLevelKeyIDForPrivateKey(priv_key);
  std::string pkcs11_id;
  if (sec_item) {
    pkcs11_id = base::HexEncode(sec_item->data, sec_item->len);
    SECITEM_FreeItem(sec_item, PR_TRUE);
  }
  SECKEY_DestroyPrivateKey(priv_key);

  return pkcs11_id;
}

void NetworkCertLoader::OnSystemNssDbReady(
    net::NSSCertDatabase* system_slot_database) {
  // SystemTokenCertDbStorage informs callers that the system token certificate
  // database initialization failed by returning nullptr.
  if (!system_slot_database) {
    LOG(ERROR) << "Failed to retrieve system token certificate database";
    return;
  }

  system_slot_cert_cache_->SetNSSDBAndSlot(
      system_slot_database, system_slot_database->GetSystemSlot(),
      true /* is_slot_device_wide */);
}

void NetworkCertLoader::OnCertCacheUpdated() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(1) << "OnCertCacheUpdated";

  if (is_shutting_down_)
    return;

  if (system_slot_cert_cache_->certificates_update_running() ||
      user_private_slot_cert_cache_->certificates_update_running() ||
      user_public_slot_cert_cache_->certificates_update_running()) {
    // Don't spam the observers - wait for the pending updates to be triggered.
    return;
  }

  certs_from_cache_loaded_ = true;
  UpdateCertificates();
}

void NetworkCertLoader::UpdateCertificates() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (is_shutting_down_)
    return;

  // Only trigger a notification to observers if one of the |CertCache|s has
  // already loaded certificates. Don't trigger notifications if policy-provided
  // certificates change before that.
  // TODO(crbug.com/40595094): Now that we handle client and authority
  // certificates separately in NetworkCertLoader, we could fire different
  // notifications for policy-provided cert changes instead of holding back
  // notifications.
  if (!certs_from_cache_loaded_)
    return;

  NetworkCertList user_policy_authorities = GetPolicyProvidedAuthorities(
      user_policy_certificate_provider_, false /* device_wide */);
  NetworkCertList device_policy_authorities = GetPolicyProvidedAuthorities(
      device_policy_certificate_provider_, true /* device_wide */);
  all_authority_certs_ = CombineNetworkCertLists(
      {&system_slot_cert_cache_->authority_certs(),
       &user_public_slot_cert_cache_->authority_certs(),
       &user_private_slot_cert_cache_->authority_certs(),
       &user_policy_authorities, &device_policy_authorities});

  all_client_certs_ =
      CombineNetworkCertLists({&system_slot_cert_cache_->client_certs(),
                               &user_public_slot_cert_cache_->client_certs(),
                               &user_private_slot_cert_cache_->client_certs()});

  VLOG(1) << "OnCertCacheUpdated (all_authority_certs="
          << all_authority_certs_.size()
          << ", all_client_certs=" << all_client_certs_.size() << ")";
  NotifyCertificatesLoaded();
}

void NetworkCertLoader::NotifyCertificatesLoaded() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (is_shutting_down_)
    return;
  for (auto& observer : observers_)
    observer.OnCertificatesLoaded();
}

void NetworkCertLoader::OnPolicyProvidedCertsChanged() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  UpdateCertificates();
}

}  // namespace ash
