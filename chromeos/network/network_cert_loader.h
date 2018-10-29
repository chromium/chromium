// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_CERT_LOADER_H_
#define CHROMEOS_NETWORK_NETWORK_CERT_LOADER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/policy_certificate_provider.h"
#include "net/cert/scoped_nss_types.h"

namespace net {
class NSSCertDatabase;
}

namespace chromeos {

// This class is responsible for loading certificates once the TPM is
// initialized. It is expected to be constructed on the UI thread and public
// methods should all be called from the UI thread.
// When certificates have been loaded (after login completes and tpm token is
// initialized), or the cert database changes, observers are called with
// OnCertificatesLoaded().
// This class supports using one or two cert databases. The expected usage is
// that NetworkCertLoader is used with a NSSCertDatabase backed by the system
// token before user sign-in, and additionally with a user-specific
// NSSCertDatabase after user sign-in. When both NSSCertDatabase are used,
// NetworkCertLoader combines certificates from both into |all_certs()|.
class CHROMEOS_EXPORT NetworkCertLoader
    : public PolicyCertificateProvider::Observer {
 public:
  class Observer {
   public:
    // Called when the certificates, passed for convenience as |all_certs|,
    // have completed loading.
    virtual void OnCertificatesLoaded(
        const net::ScopedCERTCertificateList& all_certs) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Sets the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static NetworkCertLoader* Get();

  // Returns true if the global instance has been initialized.
  static bool IsInitialized();

  // Returns the PKCS#11 attribute CKA_ID for a certificate as an upper-case
  // hex string and sets |slot_id| to the id of the containing slot, or returns
  // an empty string and doesn't modify |slot_id| if the PKCS#11 id could not be
  // determined.
  static std::string GetPkcs11IdAndSlotForCert(CERTCertificate* cert,
                                               int* slot_id);

  // When this is called, |NetworkCertLoader| stops sending out updates to its
  // observers. This is a workaround for https://crbug.com/894867, where a crash
  // is suspected to happen due to updates sent out during the shutdown
  // procedure. TODO(https://crbug.com/894867): Remove this when the root cause
  // is found.
  void set_is_shutting_down() { is_shutting_down_ = true; }

  // Sets the NSS cert database which NetworkCertLoader should use to access
  // system slot certificates. The NetworkCertLoader will _not_ take ownership
  // of the database - see comment on SetUserNSSDB. NetworkCertLoader supports
  // working with only one database or with both (system and user) databases.
  void SetSystemNSSDB(net::NSSCertDatabase* system_slot_database);

  // Sets the NSS cert database which NetworkCertLoader should use to access
  // user slot certificates. NetworkCertLoader understands the edge case that
  // this database could also give access to system slot certificates (e.g. for
  // affiliated users). The NetworkCertLoader will _not_ take the ownership of
  // the database, but it expects it to stay alive at least until the shutdown
  // starts on the main thread. This assumes that SetUserNSSDB and other methods
  // directly using |database_| are not called during shutdown.
  // NetworkCertLoader supports working with only one database or with both
  // (system and user) databases.
  void SetUserNSSDB(net::NSSCertDatabase* user_database);

  // Adds the passed |PolicyCertificateProvider| and starts using the authority
  // certificates provided by it. NetworkCertLoader registers itself as Observer
  // on |policy_certificate_provider|, so the caller must ensure to call
  // |RemovePolicyCertificateProvider| before |policy_certificate_provider| is
  // destroyed or before |NetworkCertLoader| is shut down.
  void AddPolicyCertificateProvider(
      PolicyCertificateProvider* policy_certificate_provider);

  // Removes the passed |PolicyCertificateProvider| and stops using authority
  // certificates provided by it. |policy_certificate_provider| must have been
  // added using |AddPolicyCertificateProvider| before.
  void RemovePolicyCertificateProvider(
      PolicyCertificateProvider* policy_certificate_provider);

  void AddObserver(NetworkCertLoader::Observer* observer);
  void RemoveObserver(NetworkCertLoader::Observer* observer);

  // Returns true if |cert| is hardware backed. See also
  // ForceHardwareBackedForTesting().
  static bool IsCertificateHardwareBacked(CERTCertificate* cert);

  // Returns true when the certificate list has been requested but not loaded.
  // When two databases are in use (SetSystemNSSDB and SetUserNSSDB have both
  // been called), this returns true when at least one of them is currently
  // loading certificates.
  // Note that this method poses an exception in the NetworkCertLoader
  // interface: While most of NetworkCertLoader's interface treats the initial
  // load of a second database the same way as an update in the first database,
  // this method does not. The reason is that it's targeted at displaying a
  // message in the GUI, so the user knows that (more) certificates will be
  // available soon.
  bool initial_load_of_any_database_running() const;

  // Returns true if any certificates have been loaded. If NetworkCertLoader
  // uses a system and a user NSS database, this returns true after the
  // certificates from the first (usually system) database have been loaded.
  bool initial_load_finished() const;

  // Returns true if certificates from a user NSS database have been loaded.
  bool user_cert_database_load_finished() const;

  // Returns all certificates. This will be empty until certificates_loaded() is
  // true.
  const net::ScopedCERTCertificateList& all_certs() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return all_certs_;
  }

  // Returns certificates from the system token. This will be empty until
  // certificates_loaded() is true.
  const net::ScopedCERTCertificateList& system_token_client_certs() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return system_token_client_certs_;
  }

  // Called in tests if |IsCertificateHardwareBacked()| should always return
  // true.
  static void ForceHardwareBackedForTesting();

 private:
  class CertCache;

  NetworkCertLoader();
  ~NetworkCertLoader() override;

  // Called when |system_cert_cache_| or |user_cert_cache| certificates have
  // potentially changed.
  void OnCertCacheUpdated();

  // Called as a result of |OnCertCacheUpdated|. This is a separate function,
  // because |OnCertCacheUpdated| may trigger a background task for filtering
  // certificates.
  void StoreCertsFromCache(
      net::ScopedCERTCertificateList all_certs,
      net::ScopedCERTCertificateList system_token_client_certs);

  // Called when policy-provided certificates or cache-based certificates (see
  // |all_certs_from_cache_|) have potentially changed.
  void UpdateCertificates();

  void NotifyCertificatesLoaded();

  // PolicyCertificateProvider::Observer
  void OnPolicyProvidedCertsChanged(
      const net::CertificateList& all_server_and_authority_certs,
      const net::CertificateList& trust_anchors) override;

  // If this is true, |NetworkCertLoader| does not send out notifications to its
  // observers anymore.
  bool is_shutting_down_ = false;

  base::ObserverList<Observer>::Unchecked observers_;

  // Cache for certificates from the system-token NSSCertDatabase.
  std::unique_ptr<CertCache> system_cert_cache_;
  // Cache for certificates from the user-specific NSSCertDatabase.
  std::unique_ptr<CertCache> user_cert_cache_;

  // Cached certificates loaded from the database(s) and policy-pushed Authority
  // certificates.
  net::ScopedCERTCertificateList all_certs_;

  // Cached certificates loaded from the database(s).
  net::ScopedCERTCertificateList all_certs_from_cache_;

  // Cached certificates from system token.
  net::ScopedCERTCertificateList system_token_client_certs_;

  // True if |StoreCertsFromCache()| was called before.
  bool certs_from_cache_loaded_ = false;

  std::vector<const PolicyCertificateProvider*> policy_certificate_providers_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<NetworkCertLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkCertLoader);
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_CERT_LOADER_H_
