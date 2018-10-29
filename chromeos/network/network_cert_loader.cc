// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_cert_loader.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/cert_database.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_util_nss.h"

namespace chromeos {

// Caches certificates from a NSSCertDatabase. Handles reloading of certificates
// on update notifications and provides status flags (loading / loaded).
// NetworkCertLoader can use multiple CertCaches to combine certificates from
// multiple sources.
class NetworkCertLoader::CertCache : public net::CertDatabase::Observer {
 public:
  explicit CertCache(base::RepeatingClosure certificates_updated_callback)
      : certificates_updated_callback_(certificates_updated_callback),
        weak_factory_(this) {}

  ~CertCache() override {
    net::CertDatabase::GetInstance()->RemoveObserver(this);
  }

  void SetNSSDB(net::NSSCertDatabase* nss_database) {
    CHECK(!nss_database_);
    nss_database_ = nss_database;

    // Start observing cert database for changes.
    // Observing net::CertDatabase is preferred over observing |nss_database_|
    // directly, as |nss_database_| observers receive only events generated
    // directly by |nss_database_|, so they may miss a few relevant ones.
    // TODO(tbarzic): Once singleton NSSCertDatabase is removed, investigate if
    // it would be OK to observe |nss_database_| directly; or change
    // NSSCertDatabase to send notification on all relevant changes.
    net::CertDatabase::GetInstance()->AddObserver(this);

    LoadCertificates();
  }

  net::NSSCertDatabase* nss_database() { return nss_database_; }

  // net::CertDatabase::Observer
  void OnCertDBChanged() override {
    VLOG(1) << "OnCertDBChanged";
    LoadCertificates();
  }

  const net::ScopedCERTCertificateList& cert_list() const { return cert_list_; }

  bool initial_load_running() const {
    return nss_database_ && !initial_load_finished_;
  }

  bool initial_load_finished() const { return initial_load_finished_; }

  // Returns true if the underlying NSSCertDatabase has access to the system
  // slot.
  bool has_system_certificates() const { return has_system_certificates_; }

 private:
  // Trigger a certificate load. If a certificate loading task is already in
  // progress, will start a reload once the current task is finished.
  void LoadCertificates() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    VLOG(1) << "LoadCertificates: " << certificates_update_running_;

    if (certificates_update_running_) {
      certificates_update_required_ = true;
      return;
    }

    certificates_update_running_ = true;
    certificates_update_required_ = false;

    if (nss_database_) {
      has_system_certificates_ =
          static_cast<bool>(nss_database_->GetSystemSlot());
      nss_database_->ListCerts(base::AdaptCallbackForRepeating(base::BindOnce(
          &CertCache::UpdateCertificates, weak_factory_.GetWeakPtr())));
    }
  }

  // Called if a certificate load task is finished.
  void UpdateCertificates(net::ScopedCERTCertificateList cert_list) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    DCHECK(certificates_update_running_);
    VLOG(1) << "UpdateCertificates: " << cert_list.size();

    // Ignore any existing certificates.
    cert_list_ = std::move(cert_list);

    initial_load_finished_ = true;
    certificates_updated_callback_.Run();

    certificates_update_running_ = false;
    if (certificates_update_required_)
      LoadCertificates();
  }

  // To be called when certificates have been updated.
  base::RepeatingClosure certificates_updated_callback_;

  bool has_system_certificates_ = false;

  // This is true after certificates have been loaded initially.
  bool initial_load_finished_ = false;
  // This is true if a notification about certificate DB changes arrived while
  // loading certificates and means that we will have to trigger another
  // certificates load after that.
  bool certificates_update_required_ = false;
  // This is true while certificates are being loaded.
  bool certificates_update_running_ = false;

  // The NSS certificate database from which the certificates should be loaded.
  net::NSSCertDatabase* nss_database_ = nullptr;

  // Cached Certificates loaded from the database.
  net::ScopedCERTCertificateList cert_list_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<CertCache> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CertCache);
};

namespace {

// Checks if |certificate| is on the given |slot|.
bool IsCertificateOnSlot(CERTCertificate* certificate, PK11SlotInfo* slot) {
  crypto::ScopedPK11SlotList slots_for_cert(
      PK11_GetAllSlotsForCert(certificate, nullptr));
  if (!slots_for_cert)
    return false;

  for (PK11SlotListElement* slot_element =
           PK11_GetFirstSafe(slots_for_cert.get());
       slot_element; slot_element = PK11_GetNextSafe(slots_for_cert.get(),
                                                     slot_element, PR_FALSE)) {
    if (slot_element->slot == slot) {
      // All previously visited elements have been freed by PK11_GetNextSafe,
      // but we're not calling that for the last one, so free it explicitly.
      // The slots_for_cert list itself will be freed because ScopedPK11SlotList
      // is a unique_ptr.
      PK11_FreeSlotListElement(slots_for_cert.get(), slot_element);
      return true;
    }
  }
  return false;
}

// Goes through all certificates in |certs| and copies those certificates
// which are on |system_slot| to a new list.
net::ScopedCERTCertificateList FilterSystemTokenCertificates(
    net::ScopedCERTCertificateList certs,
    crypto::ScopedPK11Slot system_slot) {
  VLOG(1) << "FilterSystemTokenCertificates";
  if (!system_slot)
    return net::ScopedCERTCertificateList();

  PK11SlotInfo* system_slot_ptr = system_slot.get();
  // Only keep certificates which are on the |system_slot|.
  certs.erase(
      std::remove_if(certs.begin(), certs.end(),
                     [system_slot_ptr](const net::ScopedCERTCertificate& cert) {
                       return !IsCertificateOnSlot(cert.get(), system_slot_ptr);
                     }),
      certs.end());
  return certs;
}

void AddPolicyProvidedAuthorities(
    const PolicyCertificateProvider* policy_certificate_provider,
    net::ScopedCERTCertificateList* out_certs) {
  DCHECK(out_certs);
  if (!policy_certificate_provider)
    return;
  for (const auto& certificate :
       policy_certificate_provider->GetAllAuthorityCertificates()) {
    net::ScopedCERTCertificate x509_cert =
        net::x509_util::CreateCERTCertificateFromX509Certificate(
            certificate.get());
    if (!x509_cert) {
      LOG(ERROR) << "Unable to create CERTCertificate";
      continue;
    }

    out_certs->push_back(std::move(x509_cert));
  }
}

}  // namespace

static NetworkCertLoader* g_cert_loader = nullptr;
static bool g_force_hardware_backed_for_test = false;

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

NetworkCertLoader::NetworkCertLoader() : weak_factory_(this) {
  system_cert_cache_ = std::make_unique<CertCache>(base::BindRepeating(
      &NetworkCertLoader::OnCertCacheUpdated, base::Unretained(this)));
  user_cert_cache_ = std::make_unique<CertCache>(base::BindRepeating(
      &NetworkCertLoader::OnCertCacheUpdated, base::Unretained(this)));
}

NetworkCertLoader::~NetworkCertLoader() {
  DCHECK(policy_certificate_providers_.empty());
}

void NetworkCertLoader::SetSystemNSSDB(
    net::NSSCertDatabase* system_slot_database) {
  system_cert_cache_->SetNSSDB(system_slot_database);
}

void NetworkCertLoader::SetUserNSSDB(net::NSSCertDatabase* user_database) {
  user_cert_cache_->SetNSSDB(user_database);
}

void NetworkCertLoader::AddPolicyCertificateProvider(
    PolicyCertificateProvider* policy_certificate_provider) {
  policy_certificate_provider->AddPolicyProvidedCertsObserver(this);
  policy_certificate_providers_.push_back(policy_certificate_provider);
  UpdateCertificates();
}

void NetworkCertLoader::RemovePolicyCertificateProvider(
    PolicyCertificateProvider* policy_certificate_provider) {
  auto iter = std::find(policy_certificate_providers_.begin(),
                        policy_certificate_providers_.end(),
                        policy_certificate_provider);
  DCHECK(iter != policy_certificate_providers_.end());
  policy_certificate_providers_.erase(iter);
  policy_certificate_provider->RemovePolicyProvidedCertsObserver(this);
  UpdateCertificates();
}

void NetworkCertLoader::AddObserver(NetworkCertLoader::Observer* observer) {
  observers_.AddObserver(observer);
}

void NetworkCertLoader::RemoveObserver(NetworkCertLoader::Observer* observer) {
  observers_.RemoveObserver(observer);
}

// static
bool NetworkCertLoader::IsCertificateHardwareBacked(CERTCertificate* cert) {
  if (g_force_hardware_backed_for_test)
    return true;
  PK11SlotInfo* slot = cert->slot;
  return slot && PK11_IsHW(slot);
}

bool NetworkCertLoader::initial_load_of_any_database_running() const {
  return system_cert_cache_->initial_load_running() ||
         user_cert_cache_->initial_load_running();
}

bool NetworkCertLoader::initial_load_finished() const {
  return system_cert_cache_->initial_load_finished() ||
         user_cert_cache_->initial_load_finished();
}

bool NetworkCertLoader::user_cert_database_load_finished() const {
  return user_cert_cache_->initial_load_finished();
}

// static
void NetworkCertLoader::ForceHardwareBackedForTesting() {
  g_force_hardware_backed_for_test = true;
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

void NetworkCertLoader::OnCertCacheUpdated() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  VLOG(1) << "OnCertCacheUpdated";

  if (is_shutting_down_)
    return;

  // If user_cert_cache_ has access to system certificates and it has already
  // finished its initial load, it will contain system certificates which we can
  // filter.
  if (user_cert_cache_->initial_load_finished() &&
      user_cert_cache_->has_system_certificates()) {
    crypto::ScopedPK11Slot system_slot =
        user_cert_cache_->nss_database()->GetSystemSlot();
    DCHECK(system_slot);
    base::PostTaskWithTraitsAndReplyWithResult(
        FROM_HERE,
        {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&FilterSystemTokenCertificates,
                       net::x509_util::DupCERTCertificateList(
                           user_cert_cache_->cert_list()),
                       std::move(system_slot)),
        base::BindOnce(&NetworkCertLoader::StoreCertsFromCache,
                       weak_factory_.GetWeakPtr(),
                       net::x509_util::DupCERTCertificateList(
                           user_cert_cache_->cert_list())));
  } else {
    // The user's cert cache does not contain system certificates.
    net::ScopedCERTCertificateList system_token_client_certs =
        net::x509_util::DupCERTCertificateList(system_cert_cache_->cert_list());
    net::ScopedCERTCertificateList all_certs_from_cache =
        net::x509_util::DupCERTCertificateList(user_cert_cache_->cert_list());
    all_certs_from_cache.reserve(all_certs_from_cache.size() +
                                 system_token_client_certs.size());
    for (const net::ScopedCERTCertificate& cert : system_token_client_certs) {
      all_certs_from_cache.push_back(
          net::x509_util::DupCERTCertificate(cert.get()));
    }
    StoreCertsFromCache(std::move(all_certs_from_cache),
                        std::move(system_token_client_certs));
  }
}

void NetworkCertLoader::StoreCertsFromCache(
    net::ScopedCERTCertificateList all_certs_from_cache,
    net::ScopedCERTCertificateList system_token_client_certs) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VLOG(1) << "StoreCertsFromCache: " << all_certs_from_cache.size() << " ("
          << system_token_client_certs.size()
          << " client certs on system slot)";

  // Ignore any existing certificates.
  all_certs_from_cache_ = std::move(all_certs_from_cache);
  system_token_client_certs_ = std::move(system_token_client_certs);

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
  // TODO(https://crbug.com/888451): When we handle client and authority
  // certificates separately in NetworkCertLoader, we could fire different
  // notifications for policy-provided cert changes instead of holding back
  // notifications. Note that it is possible that only |system_cert_cache_| has
  // loaded certificates (e.g. on the ChromeOS sign-in screen), and it is also
  // possible that only |user_cert_cache_| has loaded certificates (e.g. if the
  // system slot is not available for some reason, but a primary user has signed
  // in).
  if (!certs_from_cache_loaded_)
    return;

  // Copy |all_certs_from_cache_| into |all_certs_|, ignoring any existing
  // certificates.
  all_certs_.clear();
  all_certs_.reserve(all_certs_from_cache_.size());
  for (const net::ScopedCERTCertificate& cert : all_certs_from_cache_)
    all_certs_.push_back(net::x509_util::DupCERTCertificate(cert.get()));

  // Add policy-provided certificates.
  // TODO(https://crbug.com/888451): Instead of putting authorities and client
  // certs into |all_certs_| and then filtering in NetworkCertificateHandler, we
  // should separate the two categories here in |NetworkCertLoader| already
  // (pmarko@).
  for (const PolicyCertificateProvider* policy_certificate_provider :
       policy_certificate_providers_) {
    AddPolicyProvidedAuthorities(policy_certificate_provider, &all_certs_);
  }

  NotifyCertificatesLoaded();
}

void NetworkCertLoader::NotifyCertificatesLoaded() {
  for (auto& observer : observers_)
    observer.OnCertificatesLoaded(all_certs_);
}

void NetworkCertLoader::OnPolicyProvidedCertsChanged(
    const net::CertificateList& all_server_and_authority_certs,
    const net::CertificateList& trust_anchors) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  UpdateCertificates();
}

}  // namespace chromeos
