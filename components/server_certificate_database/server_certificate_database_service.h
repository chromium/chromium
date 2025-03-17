// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_SERVICE_H_
#define COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_SERVICE_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/server_certificate_database/server_certificate_database.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "components/server_certificate_database/server_certificate_database_nss_migrator.h"
#endif

class PrefRegistrySimple;
class PrefService;

namespace net {

#if BUILDFLAG(IS_CHROMEOS)
namespace prefs {
// Integer that indicates whether the user's NSS certificates have been
// migrated to ServerCertificateDatabase. The value is a
// ServerCertificateDatabaseService::NSSMigrationResultPref enum.
inline constexpr char kNSSCertsMigratedToServerCertDb[] =
    "certificates.nss_certs_migrated_to_server_cert_db";
}  //  namespace prefs
#endif

// KeyedService that loads and provides policies around usage of Certificates
// for TLS.
class ServerCertificateDatabaseService : public KeyedService {
 public:
#if BUILDFLAG(IS_CHROMEOS)
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class NSSMigrationResultHistogram {
    kNssDbEmpty = 0,
    kSuccess = 1,
    kPartialSuccess = 2,
    kFailed = 3,
    kMaxValue = kFailed,
  };

  // Enum that will record migration state in user's preferences. In the
  // current implementation, migration is only attempted once, but saving state
  // about whether there were any errors with the migration might be useful in
  // case there are issues during the rollout and we need to add new code that
  // can try again for anyone that had errors.
  // These values are persisted to prefs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class NSSMigrationResultPref : int {
    kNotMigrated = 0,
    kMigratedSuccessfully = 1,
    kMigrationHadErrors = 2,
  };
#endif

  using GetCertificatesCallback = base::OnceCallback<void(
      std::vector<net::ServerCertificateDatabase::CertInformation>)>;

#if BUILDFLAG(IS_CHROMEOS)
  explicit ServerCertificateDatabaseService(
      base::FilePath profile_path,
      PrefService* prefs,
      ServerCertificateDatabaseNSSMigrator::NssSlotGetter nss_slot_getter);
#else
  explicit ServerCertificateDatabaseService(base::FilePath profile_path);
#endif

  ServerCertificateDatabaseService(const ServerCertificateDatabaseService&) =
      delete;
  ServerCertificateDatabaseService& operator=(
      const ServerCertificateDatabaseService&) = delete;

  ~ServerCertificateDatabaseService() override;

  // Register a callback to be run every time the database is changed.
  base::CallbackListSubscription AddObserver(base::RepeatingClosure callback);

  // Add or update user settings with the included certificates.
  void AddOrUpdateUserCertificates(
      std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos,
      base::OnceCallback<void(bool)> callback);

  // Read all certificates from the database.
  void GetAllCertificates(GetCertificatesCallback callback);

  // Run callback with `server_cert_database_`. The callback will be run on a
  // thread pool sequence where it is allowed to call methods on the database
  // object. This can be used to do multiple operations on the database without
  // repeated thread hops.
  //
  // TODO(https://crbug.com/40928765): This does NOT notify the observer if any
  // changes were made. For the current use case (only used by the NSS
  // migrator) this does not matter, but if anything else wants to use this to
  // change the database a solution would be needed.
  void PostTaskWithDatabase(
      base::OnceCallback<void(net::ServerCertificateDatabase*)> callback);

  void GetCertificatesCount(base::OnceCallback<void(uint32_t)> callback);

  void DeleteCertificate(const std::string& sha256hash_hex,
                         base::OnceCallback<void(bool)> callback);

#if BUILDFLAG(IS_CHROMEOS)
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);
#endif

  base::WeakPtr<ServerCertificateDatabaseService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void HandleModificationResult(base::OnceCallback<void(bool)> callback,
                                bool success);

#if BUILDFLAG(IS_CHROMEOS)
  void NSSMigrationComplete(
      ServerCertificateDatabaseNSSMigrator::MigrationResult result);
#endif

  const base::FilePath profile_path_;

  base::SequenceBound<net::ServerCertificateDatabase> server_cert_database_;
#if BUILDFLAG(IS_CHROMEOS)
  raw_ptr<PrefService> prefs_;
  ServerCertificateDatabaseNSSMigrator::NssSlotGetter nss_slot_getter_;
  std::unique_ptr<ServerCertificateDatabaseNSSMigrator> nss_migrator_;
  std::vector<GetCertificatesCallback> get_certificates_pending_migration_;
#endif

  base::RepeatingClosureList observers_;

  base::WeakPtrFactory<ServerCertificateDatabaseService> weak_factory_{this};
};

}  // namespace net

#endif  // COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_SERVICE_H_
