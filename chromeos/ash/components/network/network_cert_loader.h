// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CERT_LOADER_H_
#define CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CERT_LOADER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "chromeos/ash/components/network/policy_certificate_provider.h"
#include "net/cert/scoped_nss_types.h"

namespace net {
class NSSCertDatabase;
}

namespace ash {

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
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkCertLoader
    : public PolicyCertificateProvider::Observer {
 public:
  class Observer {
   public:
    // Called when the certificates have completed loading or have been updated.
    virtual void OnCertificatesLoaded() = 0;

   protected:
    virtual ~Observer() {}
  };

  // Holds a certificate that can be used in network configs along with
  // additional information.
  class NetworkCert final {
   public:
    NetworkCert(net::ScopedCERTCertificate cert,
                bool available_for_network_auth,
                bool device_wide);
    ~NetworkCert();

    // Not copyable
    NetworkCert(const NetworkCert& other) = delete;
    NetworkCert& operator=(const NetworkCert& other) = delete;

    // Movable
    NetworkCert(NetworkCert&& other);
    NetworkCert& operator=(NetworkCert&& other);

    CERTCertificate* cert() const { return cert_.get(); }
    // Returns true if this is a client certificate that is available for
    // network authentication. authentication. See also
    // NetworkCertLoader::ForceAvailableForNetworkAuthForTesting().
    bool is_available_for_network_auth() const {
      return available_for_network_auth_;
    }
    // Returns true if this certificate is available device-wide (so it can be
    // used in shared network configs).
    bool is_device_wide() const { return device_wide_; }

    // Returns true if this certificate is hardware-backed.
    bool IsHardwareBacked() const;

    NetworkCert Clone() const;

   private:
    net::ScopedCERTCertificate cert_;
    bool available_for_network_auth_;
    bool device_wide_;
  };

  // A list of NetworkCerts.
  using NetworkCertList = std::vector<NetworkCert>;

  // Sets the global instance. Must be called before any calls to Get().
  // Note: For test usage, make sure to call
  // SystemTokenCertDbStorage::Initialize() before initializing the
  // NetworkCertLoader.
  static void Initialize();

  // Destroys the global instance.
  static void Shutdown();

  // Gets the global instance. Initialize() must be called first.
  static NetworkCertLoader* Get();

  NetworkCertLoader(const NetworkCertLoader&) = delete;
  NetworkCertLoader& operator=(const NetworkCertLoader&) = delete;

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
  // procedure. TODO(crbug.com/41420425): Remove this when the root cause
  // is found.
  void set_is_shutting_down() { is_shutting_down_ = true; }

  // Marks that the initialization of the system slot NSSCertDatabase has
  // started. The caller should call SetSystemNSSDB when the NSSCertDatabase is
  // available.
  void MarkSystemNSSDBWillBeInitialized();

  // Used by tests to set the NSS cert database which NetworkCertLoader should
  // use to access system slot certificates.
  void SetSystemNssDbForTesting(net::NSSCertDatabase* system_slot_database);

  // Marks that the initialization of the user slot NSSCertDatabase has started.
  // The caller should call SetSystemNSSDB when the NSSCertDatabase is
  // available.
  void MarkUserNSSDBWillBeInitialized();

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

  // Sets the PolicyCertificateProvider for device policy (its authority
  // certificates will be available device-wide). Call with nullptr to remove it
  // again.
  void SetDevicePolicyCertificateProvider(
      PolicyCertificateProvider* device_policy_certificate_provider);

  // Sets the PolicyCertificateProvider for user policy (its authority
  // certificates will not be available device-wide). Call with nullptr to
  // remove it again.
  void SetUserPolicyCertificateProvider(
      PolicyCertificateProvider* device_user_certificate_provider);

  void AddObserver(NetworkCertLoader::Observer* observer);
  void RemoveObserver(NetworkCertLoader::Observer* observer);

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

  // Returns true if there can be any client certificates in the current device
  // state. this means that either a system slot or a primary user's private
  // slot are present, or have been marked as being initialized.
  bool can_have_client_certificates() const;

  // Returns authority certificates usable for network configurations. This will
  // be empty until certificates_loaded() is true.
  const NetworkCertList& authority_certs() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return all_authority_certs_;
  }

  // Returns client certificates usable for network configuration. This will be
  // empty until certificates_loaded() is true.
  const NetworkCertList& client_certs() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return all_client_certs_;
  }

  // Returns all certificates from |network_cert_list|, ignoring if they're
  // device-wide or not.
  static net::ScopedCERTCertificateList GetAllCertsFromNetworkCertList(
      const NetworkCertList& network_cert_list);

  // Clones a vector of |NetworkCert|s.
  static NetworkCertList CloneNetworkCertList(
      const NetworkCertList& network_cert_list);

  // Called in tests if |NetworkCert::is_available_for_network_auth()| should
  // always return true.
  static void ForceAvailableForNetworkAuthForTesting();

 private:
  class CertCache;

  NetworkCertLoader();
  ~NetworkCertLoader() override;

  // Sets the NSS cert database which NetworkCertLoader should use to access
  // system slot certificates. The NetworkCertLoader will _not_ take ownership
  // of the database - see comment on SetUserNSSDB. NetworkCertLoader supports
  // working with only one database or with both (system and user) databases.
  // This method is passed as a callback to SystemTokenCertDbStorage which will
  // call it when the system slot database is ready or the database
  // initialization has failed.
  void OnSystemNssDbReady(net::NSSCertDatabase* system_slot_database);

  // Called when |system_cert_cache_| or |user_cert_cache| certificates have
  // potentially changed.
  void OnCertCacheUpdated();

  // Called when policy-provided certificates or cache-based certificates (see
  // |all_certs_from_cache_|) have potentially changed.
  void UpdateCertificates();

  void NotifyCertificatesLoaded();

  // PolicyCertificateProvider::Observer
  void OnPolicyProvidedCertsChanged() override;

  // If this is true, |NetworkCertLoader| does not send out notifications to its
  // observers anymore.
  bool is_shutting_down_ = false;

  base::ObserverList<Observer>::Unchecked observers_;

  // Cache for certificates from the system-token NSSCertDatabase.
  std::unique_ptr<CertCache> system_slot_cert_cache_;
  // Cache for certificates from the user-specific NSSCertDatabase, listing
  // certificates from the private slot.
  std::unique_ptr<CertCache> user_private_slot_cert_cache_;
  // Cache for certificates from the user-specific NSSCertDatabase, listing
  // certificates from the public slot.
  std::unique_ptr<CertCache> user_public_slot_cert_cache_;

  // Client certificates.
  NetworkCertList all_client_certs_;

  // Authority certs from |cached_certs_| extended by authority certs provided
  // by the policy certificate providers.
  NetworkCertList all_authority_certs_;

  // True if |StoreCertsFromCache()| was called before.
  bool certs_from_cache_loaded_ = false;

  raw_ptr<PolicyCertificateProvider> device_policy_certificate_provider_ =
      nullptr;
  raw_ptr<PolicyCertificateProvider> user_policy_certificate_provider_ =
      nullptr;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<NetworkCertLoader> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_NETWORK_NETWORK_CERT_LOADER_H_
