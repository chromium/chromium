// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_H_
#define COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/server_certificate_database/server_certificate_database.pb.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "third_party/boringssl/src/pki/trust_store.h"

namespace net {

extern const base::FilePath::CharType kServerCertificateDatabaseName[];

// Wraps the SQLite database that provides on-disk storage for user-configured
// TLS certificates. This class is expected to be created and accessed on a
// backend sequence.
class ServerCertificateDatabase {
 public:
  struct CertInformation {
    // Initializes a CertInformation object with the `der_cert` and calculates
    // the `sha256hash_hex` from the supplied cert.
    explicit CertInformation(base::span<const uint8_t> cert);
    CertInformation();
    ~CertInformation();
    CertInformation(CertInformation&&);
    CertInformation& operator=(CertInformation&& other);

    std::string sha256hash_hex;
    std::vector<uint8_t> der_cert;
    chrome_browser_server_certificate_database::CertificateMetadata
        cert_metadata;
  };

  // Opens the database in `storage_dir`, creating it if one does not exist.
  // `storage_dir` will generally be in the Profile directory.
  explicit ServerCertificateDatabase(const base::FilePath& storage_dir);

  ServerCertificateDatabase(const ServerCertificateDatabase&) = delete;
  ServerCertificateDatabase& operator=(const ServerCertificateDatabase&) =
      delete;
  ~ServerCertificateDatabase();

  static std::optional<bssl::CertificateTrustType> GetUserCertificateTrust(
      const net::ServerCertificateDatabase::CertInformation& cert_info);

  // Insert new certificates into the database, or if some of the certificates
  // are already present (as indicated by cert_info.sha256hash_hex), update the
  // entry in the database.
  bool InsertOrUpdateCerts(std::vector<CertInformation> cert_infos);

  // Retrieve all of the certificates from the database.
  std::vector<CertInformation> RetrieveAllCertificates();

  uint32_t RetrieveCertificatesCount();

  // Delete the certificate with a matching hash from the database.
  bool DeleteCertificate(const std::string& sha256hash_hex);

 private:
  sql::InitStatus InitInternal(const base::FilePath& storage_dir);

  // The underlying SQL database.
  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);

  // If the DB was successfully initialized. This is used to ensure that we
  // don't crash if the DB is unable to be initialized (e.g if Chrome is being
  // run from a read-only volume).
  bool db_initialized_ = false;
};

}  // namespace net

#endif  // COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_H_
