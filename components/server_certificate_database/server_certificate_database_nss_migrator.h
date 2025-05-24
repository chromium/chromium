// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_NSS_MIGRATOR_H_
#define COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_NSS_MIGRATOR_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/internal/platform_trust_store.h"

namespace net {

class ServerCertificateDatabaseService;

// Migrates server-related certificates from an NSS user database into
// ServerCertificateDatabase.
// Does not migrate client certificates as those are handled by Kcer and a
// separate migration process handles those.
class ServerCertificateDatabaseNSSMigrator {
 public:
  struct MigrationResult {
    // The number of certs that were read from the user's NSS database.
    int cert_count = 0;

    // Count of how many certs failed to import into the user's
    // ServerCertDatabase.
    int error_count = 0;
  };
  using ResultCallback = base::OnceCallback<void(MigrationResult)>;

  // A callback that is used to get a NSS slot handle. The callback is passed a
  // result callback which will be called with the slot, possibly
  // asynchronously and possibly on an arbitrary thread (need not be the same
  // thread the NssSlotGetter was run on).
  using NssSlotGetter = base::OnceCallback<void(
      base::OnceCallback<void(crypto::ScopedPK11Slot)>)>;

  explicit ServerCertificateDatabaseNSSMigrator(
      ServerCertificateDatabaseService* cert_db_service,
      NssSlotGetter nss_slot_getter);
  ~ServerCertificateDatabaseNSSMigrator();

  // Begins migration process. `callback` will be run on the calling thread when
  // the migration is complete. Must be called on the UI thread.
  // If the ServerCertificateDatabaseNSSMigrator is deleted before the callback
  // has been run, the callback will not be run and migrated certs may or may
  // not be written to the ServerCertificateDatabase.
  void MigrateCerts(ResultCallback callback);

 private:
  void GotCertsFromNSS(
      ResultCallback callback,
      std::vector<net::PlatformTrustStore::CertWithTrust> certs_to_migrate);
  void FinishedMigration(ResultCallback callback, MigrationResult result);

  raw_ptr<ServerCertificateDatabaseService> cert_db_service_
      GUARDED_BY_CONTEXT(sequence_checker_);
  NssSlotGetter nss_slot_getter_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ServerCertificateDatabaseNSSMigrator> weak_ptr_factory_{
      this};
};

}  // namespace net

#endif  // COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_NSS_MIGRATOR_H_
