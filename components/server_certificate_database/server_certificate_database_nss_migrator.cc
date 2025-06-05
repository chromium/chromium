// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/server_certificate_database/server_certificate_database_nss_migrator.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "components/server_certificate_database/server_certificate_database_service.h"
#include "crypto/scoped_nss_types.h"
#include "crypto/sha2.h"
#include "net/cert/internal/trust_store_nss.h"

namespace net {

namespace {

auto MapTrust(const bssl::CertificateTrust& trust) {
  if (trust.IsDistrusted()) {
    return chrome_browser_server_certificate_database::CertificateTrust::
        CERTIFICATE_TRUST_TYPE_DISTRUSTED;
  }
  if (trust.IsTrustAnchor() || trust.IsTrustLeaf()) {
    return chrome_browser_server_certificate_database::CertificateTrust::
        CERTIFICATE_TRUST_TYPE_TRUSTED;
  }
  return chrome_browser_server_certificate_database::CertificateTrust::
      CERTIFICATE_TRUST_TYPE_UNSPECIFIED;
}

void MigrateCertsOnBackgroundThread(
    std::vector<net::PlatformTrustStore::CertWithTrust> certs_to_migrate,
    ServerCertificateDatabaseNSSMigrator::ResultCallback callback,
    net::ServerCertificateDatabase* server_cert_database) {
  ServerCertificateDatabaseNSSMigrator::MigrationResult result;
  result.cert_count = certs_to_migrate.size();
  for (net::PlatformTrustStore::CertWithTrust& cert_to_migrate :
       certs_to_migrate) {
    net::ServerCertificateDatabase::CertInformation cert_info(
        cert_to_migrate.cert_bytes);
    cert_info.cert_metadata.mutable_trust()->set_trust_type(
        MapTrust(cert_to_migrate.trust));

    std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos;
    cert_infos.push_back(std::move(cert_info));
    bool ok = server_cert_database->InsertOrUpdateCerts(std::move(cert_infos));
    if (!ok) {
      result.error_count++;
      LOG(ERROR) << "error importing cert " << cert_info.sha256hash_hex;
    }
  }

  std::move(callback).Run(std::move(result));
}

void ReadNSSCertsOnBackgroundThread(
    crypto::ScopedPK11Slot slot,
    base::OnceCallback<
        void(std::vector<net::PlatformTrustStore::CertWithTrust>)> callback) {
  TrustStoreNSS trust_store_nss(
      TrustStoreNSS::UserSlotTrustSetting(std::move(slot)));
  std::move(callback).Run(trust_store_nss.GetAllUserAddedCerts());
}

void GotNssSlot(
    base::OnceCallback<
        void(std::vector<net::PlatformTrustStore::CertWithTrust>)> callback,
    crypto::ScopedPK11Slot slot) {
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ReadNSSCertsOnBackgroundThread, std::move(slot),
                     std::move(callback)));
}

}  // namespace

ServerCertificateDatabaseNSSMigrator::ServerCertificateDatabaseNSSMigrator(
    net::ServerCertificateDatabaseService* cert_db_service,
    NssSlotGetter nss_slot_getter)
    : cert_db_service_(cert_db_service),
      nss_slot_getter_(std::move(nss_slot_getter)) {}

ServerCertificateDatabaseNSSMigrator::~ServerCertificateDatabaseNSSMigrator() =
    default;

void ServerCertificateDatabaseNSSMigrator::MigrateCerts(
    ResultCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(nss_slot_getter_)
      .Run(base::BindOnce(
          &GotNssSlot,
          base::BindPostTaskToCurrentDefault(base::BindOnce(
              &ServerCertificateDatabaseNSSMigrator::GotCertsFromNSS,
              weak_ptr_factory_.GetWeakPtr(), std::move(callback)))));
}

void ServerCertificateDatabaseNSSMigrator::GotCertsFromNSS(
    ResultCallback callback,
    std::vector<net::PlatformTrustStore::CertWithTrust> certs_to_migrate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cert_db_service_->PostTaskWithDatabase(base::BindOnce(
      &MigrateCertsOnBackgroundThread, std::move(certs_to_migrate),
      base::BindPostTaskToCurrentDefault(base::BindOnce(
          &ServerCertificateDatabaseNSSMigrator::FinishedMigration,
          weak_ptr_factory_.GetWeakPtr(), std::move(callback)))));
}

void ServerCertificateDatabaseNSSMigrator::FinishedMigration(
    ResultCallback callback,
    MigrationResult result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(std::move(result));
  // `this` object may be deleted by the callback, do not access object after
  // this point.
}

}  // namespace net
