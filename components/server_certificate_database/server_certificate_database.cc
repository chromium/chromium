// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/server_certificate_database/server_certificate_database.h"

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/zip.h"
#include "build/build_config.h"
#include "crypto/sha2.h"
#include "net/cert/x509_util.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

namespace net {

extern const base::FilePath::CharType kServerCertificateDatabaseName[] =
    FILE_PATH_LITERAL("ServerCertificate");

// These database versions should roll together unless we develop migrations.
constexpr int kLowestSupportedDatabaseVersion = 1;
constexpr int kCurrentDatabaseVersion = 1;

namespace {

[[nodiscard]] bool CreateTable(sql::Database& db) {
  static constexpr char kSqlCreateTablePassages[] =
      "CREATE TABLE IF NOT EXISTS certificates("
      // sha256 hash (in hex) of certificate.
      "sha256hash_hex TEXT PRIMARY KEY,"
      // The certificate, DER-encoded.
      "der_cert BLOB NOT NULL,"
      // Trust settings for the certificate.
      "trust_settings BLOB NOT NULL);";

  return db.Execute(kSqlCreateTablePassages);
}

}  // namespace

ServerCertificateDatabase::ServerCertificateDatabase(
    const base::FilePath& storage_dir)
    : db_(/*tag=*/"ServerCertificate") {
  auto status = InitInternal(storage_dir);

  db_initialized_ = (status == sql::InitStatus::INIT_OK);
}

ServerCertificateDatabase::~ServerCertificateDatabase() = default;

sql::InitStatus ServerCertificateDatabase::InitInternal(
    const base::FilePath& storage_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::FilePath db_file_path =
      storage_dir.Append(kServerCertificateDatabaseName);
  if (!db_.Open(db_file_path)) {
    return sql::InitStatus::INIT_FAILURE;
  }

  // Raze old incompatible databases.
  if (sql::MetaTable::RazeIfIncompatible(&db_, kLowestSupportedDatabaseVersion,
                                         kCurrentDatabaseVersion) ==
      sql::RazeIfIncompatibleResult::kFailed) {
    return sql::InitStatus::INIT_FAILURE;
  }

  // Wrap initialization in a transaction to make it atomic.
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return sql::InitStatus::INIT_FAILURE;
  }

  // Initialize the current version meta table. Safest to leave the compatible
  // version equal to the current version - unless we know we're making a very
  // safe backwards-compatible schema change.
  sql::MetaTable meta_table;
  if (!meta_table.Init(&db_, kCurrentDatabaseVersion,
                       /*compatible_version=*/kCurrentDatabaseVersion)) {
    return sql::InitStatus::INIT_FAILURE;
  }
  if (meta_table.GetCompatibleVersionNumber() > kCurrentDatabaseVersion) {
    return sql::INIT_TOO_NEW;
  }

  if (!CreateTable(db_)) {
    return sql::INIT_FAILURE;
  }

  if (!transaction.Commit()) {
    return sql::INIT_FAILURE;
  }

  return sql::InitStatus::INIT_OK;
}

bool ServerCertificateDatabase::InsertOrUpdateCerts(
    std::vector<CertInformation> cert_infos) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't crash browser if DB isn't initialized.
  if (!db_initialized_) {
    return false;
  }

  std::vector<std::string> proto_bytes_vec;

  // Quick check to ensure we can serialize all of the bytes before starting
  // the transaction.
  for (const CertInformation& cert_info : cert_infos) {
    std::string proto_bytes;
    // If we can't serialize the proto to an array for some reason, bail.
    if (!cert_info.cert_metadata.SerializeToString(&proto_bytes)) {
      return false;
    }
    proto_bytes_vec.push_back(std::move(proto_bytes));
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  for (auto&& [cert_info, proto_bytes] :
       base::zip(cert_infos, proto_bytes_vec)) {
    sql::Statement insert_statement(db_.GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT OR REPLACE INTO certificates(sha256hash_hex, der_cert, "
        "trust_settings) VALUES(?,?,?)"));
    insert_statement.BindString(0, cert_info.sha256hash_hex);
    insert_statement.BindBlob(1, std::move(cert_info.der_cert));
    insert_statement.BindBlob(2, std::move(proto_bytes));
    if (!insert_statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

std::vector<ServerCertificateDatabase::CertInformation>
ServerCertificateDatabase::RetrieveAllCertificates() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't crash browser if DB isn't initialized.
  if (!db_initialized_) {
    return {};
  }

  std::vector<ServerCertificateDatabase::CertInformation> certs;
  static constexpr char kSqlSelectAllCerts[] =
      "SELECT sha256hash_hex, der_cert, trust_settings FROM certificates";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectAllCerts));
  DCHECK(statement.is_valid());
  while (statement.Step()) {
    ServerCertificateDatabase::CertInformation cert_info;
    cert_info.sha256hash_hex = statement.ColumnString(0);
    cert_info.der_cert = statement.ColumnBlobAsVector(1);

    std::string trust_bytes = statement.ColumnBlobAsString(2);

    if (cert_info.cert_metadata.ParseFromString(trust_bytes)) {
      certs.push_back(std::move(cert_info));
    }
  }

  return certs;
}

uint32_t ServerCertificateDatabase::RetrieveCertificatesCount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't crash browser if DB isn't initialized.
  if (!db_initialized_) {
    return 0;
  }

  static constexpr char kSqlSelectCertsCount[] =
      "SELECT COUNT(*) FROM certificates";
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectCertsCount));
  DCHECK(statement.is_valid());
  if (!statement.Step()) {
    return 0;
  }
  return statement.ColumnInt(0);
}

bool ServerCertificateDatabase::DeleteCertificate(
    const std::string& sha256hash_hex) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Don't crash browser if DB isn't initialized.
  if (!db_initialized_) {
    return false;
  }

  sql::Statement delete_statement(db_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM certificates WHERE sha256hash_hex=?"));
  DCHECK(delete_statement.is_valid());
  delete_statement.BindString(0, sha256hash_hex);
  return delete_statement.Run() && db_.GetLastChangeCount() > 0;
}

ServerCertificateDatabase::CertInformation::CertInformation(
    base::span<const uint8_t> cert) {
  der_cert = base::ToVector(cert);
  sha256hash_hex = base::HexEncodeLower(crypto::SHA256Hash(cert));
}
ServerCertificateDatabase::CertInformation::CertInformation() = default;
ServerCertificateDatabase::CertInformation::~CertInformation() = default;
ServerCertificateDatabase::CertInformation::CertInformation(
    ServerCertificateDatabase::CertInformation&&) = default;
ServerCertificateDatabase::CertInformation&
ServerCertificateDatabase::CertInformation::operator=(
    ServerCertificateDatabase::CertInformation&& other) = default;

std::optional<bssl::CertificateTrustType>
ServerCertificateDatabase::GetUserCertificateTrust(
    const net::ServerCertificateDatabase::CertInformation& cert_info) {
  using chrome_browser_server_certificate_database::CertificateTrust;
  switch (cert_info.cert_metadata.trust().trust_type()) {
    case CertificateTrust::CERTIFICATE_TRUST_TYPE_UNSPECIFIED:
      return bssl::CertificateTrustType::UNSPECIFIED;
    case CertificateTrust::CERTIFICATE_TRUST_TYPE_DISTRUSTED:
      return bssl::CertificateTrustType::DISTRUSTED;
    case CertificateTrust::CERTIFICATE_TRUST_TYPE_TRUSTED: {
      auto parsed = bssl::ParsedCertificate::Create(
          net::x509_util::CreateCryptoBuffer(cert_info.der_cert),
          net::x509_util::DefaultParseCertificateOptions(), nullptr);

      if (!parsed) {
        return std::nullopt;
      }

      // If basic constraints are missing, assume that is_ca is set.
      bool isCA = !parsed->has_basic_constraints() ||
                  (parsed->has_basic_constraints() &&
                   parsed->basic_constraints().is_ca);

      bool has_names = parsed->has_subject_alt_names() &&
                       !(parsed->subject_alt_names()->dns_names.empty() &&
                         parsed->subject_alt_names()->ip_addresses.empty());

      if (isCA) {
        if (has_names) {
          return bssl::CertificateTrustType::TRUSTED_ANCHOR_OR_LEAF;
        } else {
          return bssl::CertificateTrustType::TRUSTED_ANCHOR;
        }
      }

      // isCA = false
      if (has_names) {
        return bssl::CertificateTrustType::TRUSTED_LEAF;
      } else {
        return std::nullopt;
      }
    }
    case CertificateTrust::CERTIFICATE_TRUST_TYPE_UNKNOWN:
    default:
      return std::nullopt;
  }
}

}  // namespace net
