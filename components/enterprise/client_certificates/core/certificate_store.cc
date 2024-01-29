// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/certificate_store.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/pickle.h"
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

// Called from `GetIdentityInner` when a private key was loaded into a
// PrivateKey instance from its proto format. `private_key` will contain the
// PrivateKey instance when successful, or nullptr when not. `identity_name` and
// `certificate` are forwarded along as they are required to construct a
// `ClientIdentity` object. `callback` is used to return the end result to the
// original caller.
void OnKeyLoaded(
    const std::string& identity_name,
    scoped_refptr<net::X509Certificate> certificate,
    base::OnceCallback<void(std::optional<ClientIdentity>)> callback,
    scoped_refptr<PrivateKey> private_key) {
  if (!private_key) {
    // Loading of the private key has failed, so return an overall failure.
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(ClientIdentity(identity_name, std::move(private_key),
                                         std::move(certificate)));
}

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
                         base::OnceCallback<void(bool)> callback) override;
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

  // Will wait for the database to be initialized and then retrieve the entry
  // with `identity_name`. Will invoke `callback` with the success/fail state
  // and the entry if successful (or nullptr if not).
  void WaitForInitializationAndGetIdentityProto(
      const std::string& identity_name,
      base::OnceCallback<
          void(bool, std::unique_ptr<client_certificates_pb::ClientIdentity>)>
          callback);

  // Will get the proto ClientIdentity object stored in the database with key
  // `identity_name` and return it via `callback;
  void GetIdentityProto(
      const std::string& identity_name,
      base::OnceCallback<
          void(bool, std::unique_ptr<client_certificates_pb::ClientIdentity>)>
          callback);

  // Inner functions of the main interface functions, which contains the main
  // business logic and can be called asynchronously after certain conditions
  // are met (e.g. database initialization).
  void CreatePrivateKeyInner(
      const std::string& identity_name,
      base::OnceCallback<void(scoped_refptr<PrivateKey>)> callback,
      bool preconditions_success,
      std::unique_ptr<client_certificates_pb::ClientIdentity> proto_identity);
  void CommitCertificateInner(
      const std::string& identity_name,
      scoped_refptr<net::X509Certificate> certificate,
      base::OnceCallback<void(bool)> callback,
      bool preconditions_success,
      std::unique_ptr<client_certificates_pb::ClientIdentity> proto_identity);
  void GetIdentityInner(
      const std::string& identity_name,
      base::OnceCallback<void(std::optional<ClientIdentity>)> callback,
      bool preconditions_success,
      std::unique_ptr<client_certificates_pb::ClientIdentity> proto_identity);

  // Called when a private key was created from `CreatePrivateKeyInner`.
  // `private_key` will contain the created private key when successful, or
  // nullptr when not.
  void OnPrivateKeyCreated(
      const std::string& identity_name,
      base::OnceCallback<void(scoped_refptr<PrivateKey>)> callback,
      scoped_refptr<PrivateKey> private_key);

  // Called when a private key was saved into the database from
  // `OnPrivateKeyCreated`. `save_success` indicates whether the database's
  // update was successful or not.
  void OnPrivateKeySaved(
      scoped_refptr<PrivateKey> private_key,
      base::OnceCallback<void(scoped_refptr<PrivateKey>)> callback,
      bool save_success);

  std::vector<base::OnceClosure> pending_operations_;

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
  WaitForInitializationAndGetIdentityProto(
      identity_name,
      base::BindOnce(&CertificateStoreImpl::CreatePrivateKeyInner,
                     weak_factory_.GetWeakPtr(), identity_name,
                     std::move(callback)));
}

void CertificateStoreImpl::CommitCertificate(
    const std::string& identity_name,
    scoped_refptr<net::X509Certificate> certificate,
    base::OnceCallback<void(bool)> callback) {
  WaitForInitializationAndGetIdentityProto(
      identity_name,
      base::BindOnce(&CertificateStoreImpl::CommitCertificateInner,
                     weak_factory_.GetWeakPtr(), identity_name,
                     std::move(certificate), std::move(callback)));
}

void CertificateStoreImpl::GetIdentity(
    const std::string& identity_name,
    base::OnceCallback<void(std::optional<ClientIdentity>)> callback) {
  WaitForInitializationAndGetIdentityProto(
      identity_name, base::BindOnce(&CertificateStoreImpl::GetIdentityInner,
                                    weak_factory_.GetWeakPtr(), identity_name,
                                    std::move(callback)));
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
  database_state_ = status == leveldb_proto::Enums::InitStatus::kOK
                        ? DatabaseState::kInitialized
                        : DatabaseState::kUninitialized;

  for (auto& operation : pending_operations_) {
    std::move(operation).Run();
  }

  pending_operations_.clear();
}

void CertificateStoreImpl::WaitForInitializationAndGetIdentityProto(
    const std::string& identity_name,
    base::OnceCallback<
        void(bool, std::unique_ptr<client_certificates_pb::ClientIdentity>)>
        callback) {
  auto get_operation = base::BindOnce(&CertificateStoreImpl::GetIdentityProto,
                                      weak_factory_.GetWeakPtr(), identity_name,
                                      std::move(callback));

  if (database_state_ == DatabaseState::kInitialized) {
    std::move(get_operation).Run();
  }

  pending_operations_.push_back(std::move(get_operation));

  if (database_state_ == DatabaseState::kUninitialized) {
    InitializeDatabase();
  }
}

void CertificateStoreImpl::GetIdentityProto(
    const std::string& identity_name,
    base::OnceCallback<
        void(bool, std::unique_ptr<client_certificates_pb::ClientIdentity>)>
        callback) {
  if (database_state_ != DatabaseState::kInitialized || identity_name.empty()) {
    std::move(callback).Run(false, nullptr);
    return;
  }

  database_->GetEntry(identity_name, std::move(callback));
}

void CertificateStoreImpl::CreatePrivateKeyInner(
    const std::string& identity_name,
    base::OnceCallback<void(scoped_refptr<PrivateKey>)> callback,
    bool preconditions_success,
    std::unique_ptr<client_certificates_pb::ClientIdentity> proto_identity) {
  if (!preconditions_success) {
    std::move(callback).Run(nullptr);
    return;
  }

  if (proto_identity && (proto_identity->has_private_key() ||
                         proto_identity->has_certificate())) {
    // An identity already exists, this request is therefore treated as a
    // conflict.
    std::move(callback).Run(nullptr);
    return;
  }

  // In order:
  // 1) Create a private key,
  // 2) Convert the private key to its proto format,
  // 3) Save the private key,
  // 4) Reply to the original callback.
  key_factory_->CreatePrivateKey(base::BindOnce(
      &CertificateStoreImpl::OnPrivateKeyCreated, weak_factory_.GetWeakPtr(),
      identity_name, std::move(callback)));
}

void CertificateStoreImpl::OnPrivateKeyCreated(
    const std::string& identity_name,
    base::OnceCallback<void(scoped_refptr<PrivateKey>)> callback,
    scoped_refptr<PrivateKey> private_key) {
  if (!private_key) {
    // Failed to create a private key.
    std::move(callback).Run(nullptr);
    return;
  }

  // Commit the key to the database before returning it.
  client_certificates_pb::ClientIdentity identity_to_save;
  *identity_to_save.mutable_private_key() = private_key->ToProto();
  auto entries_to_save = std::make_unique<leveldb_proto::ProtoDatabase<
      client_certificates_pb::ClientIdentity>::KeyEntryVector>();
  entries_to_save->push_back({identity_name, std::move(identity_to_save)});

  auto entries_to_remove = std::make_unique<std::vector<std::string>>();

  database_->UpdateEntries(
      std::move(entries_to_save), std::move(entries_to_remove),
      base::BindOnce(&CertificateStoreImpl::OnPrivateKeySaved,
                     weak_factory_.GetWeakPtr(), std::move(private_key),
                     std::move(callback)));
}

void CertificateStoreImpl::OnPrivateKeySaved(
    scoped_refptr<PrivateKey> private_key,
    base::OnceCallback<void(scoped_refptr<PrivateKey>)> callback,
    bool save_success) {
  if (!save_success) {
    // Failed to save the key in the database, so don't return the private key.
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(private_key);
}

void CertificateStoreImpl::CommitCertificateInner(
    const std::string& identity_name,
    scoped_refptr<net::X509Certificate> certificate,
    base::OnceCallback<void(bool)> callback,
    bool preconditions_success,
    std::unique_ptr<client_certificates_pb::ClientIdentity> proto_identity) {
  if (!preconditions_success || !certificate) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  if (!proto_identity) {
    // If no identity exists, create one. This simply means a public certificate
    // will be stored with no corresponding private key.
    proto_identity = std::make_unique<client_certificates_pb::ClientIdentity>();
  }

  // Add cert to proto_identity, and save it to DB.
  base::Pickle pickle;
  certificate->Persist(&pickle);
  *proto_identity->mutable_certificate() =
      std::string(pickle.data_as_char(), pickle.size());

  auto entries_to_save = std::make_unique<leveldb_proto::ProtoDatabase<
      client_certificates_pb::ClientIdentity>::KeyEntryVector>();
  entries_to_save->push_back({identity_name, *proto_identity});

  auto entries_to_remove = std::make_unique<std::vector<std::string>>();

  database_->UpdateEntries(std::move(entries_to_save),
                           std::move(entries_to_remove), std::move(callback));
}

void CertificateStoreImpl::GetIdentityInner(
    const std::string& identity_name,
    base::OnceCallback<void(std::optional<ClientIdentity>)> callback,
    bool preconditions_success,
    std::unique_ptr<client_certificates_pb::ClientIdentity> proto_identity) {
  if (!preconditions_success || !proto_identity) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  scoped_refptr<net::X509Certificate> certificate = nullptr;
  if (proto_identity->has_certificate()) {
    base::Pickle pickle(proto_identity->certificate().data(),
                        proto_identity->certificate().length());
    base::PickleIterator iter(pickle);
    certificate = net::X509Certificate::CreateFromPickle(&iter);
  }

  if (!proto_identity->has_private_key()) {
    std::move(callback).Run(ClientIdentity(
        identity_name, /*private_key=*/nullptr, std::move(certificate)));
    return;
  }

  key_factory_->LoadPrivateKey(
      proto_identity->private_key(),
      base::BindOnce(OnKeyLoaded, identity_name, std::move(certificate),
                     std::move(callback)));
}

}  // namespace client_certificates
