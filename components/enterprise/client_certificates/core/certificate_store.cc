// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/certificate_store.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"
#include "components/enterprise/client_certificates/proto/client_certificates_database.pb.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "net/cert/x509_certificate.h"

namespace client_certificates {

namespace {
const base::FilePath::StringPieceType kClientCertsDbPath =
    FILE_PATH_LITERAL("ClientCertificates");
}  // namespace

class CertificateStoreImpl : public CertificateStore {
 public:
  CertificateStoreImpl(std::unique_ptr<leveldb_proto::ProtoDatabase<
                           client_certificates_pb::ClientIdentity>> database,
                       std::unique_ptr<PrivateKeyFactory> key_factory);

  ~CertificateStoreImpl() override;

  // CertificateStore:
  void CreatePrivateKey(
      const std::string& identity_name,
      base::OnceCallback<void(scoped_refptr<PrivateKey>)> callback) override;
  void CommitCertificate(const std::string& identity_name,
                         scoped_refptr<net::X509Certificate> certificate,
                         base::OnceClosure done_callback) override;
  void GetIdentity(const std::string& identity_name,
                   base::OnceCallback<void(std::optional<ClientIdentity>)>
                       callback) override;

 private:
  enum class DatabaseState {
    kUninitialized = 0,
    kInitializing = 1,
    kInitialized = 2,
  };

  // Will start the initialization of the Database. Is a no-op is the database
  // is already initialized.
  void InitializeDatabase();

  // Invoked as callback when the database is done initializing with `status` as
  // result.
  void OnDatabaseInitialized(leveldb_proto::Enums::InitStatus status);

  std::unique_ptr<
      leveldb_proto::ProtoDatabase<client_certificates_pb::ClientIdentity>>
      database_;
  std::unique_ptr<PrivateKeyFactory> key_factory_;
  DatabaseState database_state_{DatabaseState::kUninitialized};

  base::WeakPtrFactory<CertificateStoreImpl> weak_factory_{this};
};

CertificateStore::CertificateStore() = default;
CertificateStore::~CertificateStore() = default;

// static
std::unique_ptr<CertificateStore> CertificateStore::Create(
    const base::FilePath& profile_dir,
    leveldb_proto::ProtoDatabaseProvider* database_provider,
    std::unique_ptr<PrivateKeyFactory> key_factory) {
  if (!database_provider) {
    return nullptr;
  }

  auto database =
      database_provider->GetDB<client_certificates_pb::ClientIdentity>(
          leveldb_proto::ProtoDbType::CLIENT_CERTIFICATES_DATABASE,
          profile_dir.Append(kClientCertsDbPath),
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_BLOCKING}));

  return std::make_unique<CertificateStoreImpl>(std::move(database),
                                                std::move(key_factory));
}

// static
std::unique_ptr<CertificateStore> CertificateStore::CreateForTesting(
    std::unique_ptr<leveldb_proto::ProtoDatabase<
        client_certificates_pb::ClientIdentity>> database,
    std::unique_ptr<PrivateKeyFactory> key_factory) {
  return std::make_unique<CertificateStoreImpl>(std::move(database),
                                                std::move(key_factory));
}

CertificateStoreImpl::CertificateStoreImpl(
    std::unique_ptr<leveldb_proto::ProtoDatabase<
        client_certificates_pb::ClientIdentity>> database,
    std::unique_ptr<PrivateKeyFactory> key_factory)
    : database_(std::move(database)), key_factory_(std::move(key_factory)) {
  CHECK(database_);
  CHECK(key_factory_);

  InitializeDatabase();
}

CertificateStoreImpl::~CertificateStoreImpl() = default;

void CertificateStoreImpl::CreatePrivateKey(
    const std::string& identity_name,
    base::OnceCallback<void(scoped_refptr<PrivateKey>)> callback) {
  // TODO(b:319256086): Add business logic and queue operations if the database
  // is not initialized.
}

void CertificateStoreImpl::CommitCertificate(
    const std::string& identity_name,
    scoped_refptr<net::X509Certificate> certificate,
    base::OnceClosure done_callback) {
  // TODO(b:319256086): Add business logic and queue operations if the database
  // is not initialized.
}

void CertificateStoreImpl::GetIdentity(
    const std::string& identity_name,
    base::OnceCallback<void(std::optional<ClientIdentity>)> callback) {
  // TODO(b:319256086): Add business logic and queue operations if the database
  // is not initialized.
}

void CertificateStoreImpl::InitializeDatabase() {
  if (database_state_ != DatabaseState::kUninitialized) {
    return;
  }

  database_state_ = DatabaseState::kInitializing;
  database_->Init(base::BindOnce(&CertificateStoreImpl::OnDatabaseInitialized,
                                 weak_factory_.GetWeakPtr()));
}

void CertificateStoreImpl::OnDatabaseInitialized(
    leveldb_proto::Enums::InitStatus status) {
  // TODO(b:319256086): Add logic to handle operations that were queued while
  // the DB was initializing.
  // TODO(b:319256086): Add logic to handle error DB initialization status.
  database_state_ = status == leveldb_proto::Enums::InitStatus::kOK
                        ? DatabaseState::kInitialized
                        : DatabaseState::kUninitialized;
}

}  // namespace client_certificates
