// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/leveldb_certificate_store.h"

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/pickle.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/client_certificates/core/metrics_util.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"
#include "components/enterprise/client_certificates/proto/client_certificates_database.pb.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/leveldb_proto/public/shared_proto_database_client_list.h"
#include "net/cert/x509_certificate.h"

namespace client_certificates {

namespace {
const base::FilePath::StringViewType kClientCertsDbPath =
    FILE_PATH_LITERAL("ClientCertificates");

void OnIdentityFetched(
    base::OnceCallback<void(
        StoreErrorOr<std::unique_ptr<client_certificates_pb::ClientIdentity>>)>
        callback,
    bool success,
    std::unique_ptr<client_certificates_pb::ClientIdentity> identity) {
  if (!success) {
    std::move(callback).Run(
        base::unexpected(StoreError::kGetDatabaseEntryFailed));
    return;
  }

  std::move(callback).Run(std::move(identity));
}

// Called from `GetIdentityInner` when a private key was loaded into a
// PrivateKey instance from its proto format. `private_key` will contain the
// PrivateKey instance when successful, or nullptr when not. `identity_name` and
// `certificate` are forwarded along as they are required to construct a
// `ClientIdentity` object. `callback` is used to return the end result to the
// original caller.
void OnKeyLoaded(
    const std::string& identity_name,
    scoped_refptr<net::X509Certificate> certificate,
    base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>
        callback,
    scoped_refptr<PrivateKey> private_key) {
  if (!private_key) {
    // Loading of the private key has failed, so return an overall failure.
    std::move(callback).Run(base::unexpected(StoreError::kLoadKeyFailed));
    return;
  }
  std::move(callback).Run(ClientIdentity(identity_name, std::move(private_key),
                                         std::move(certificate)));
}

void OnCertificateCommitted(
    base::OnceCallback<void(std::optional<StoreError>)> callback,
    bool success) {
  if (!success) {
    std::move(callback).Run(StoreError::kCertificateCommitFailed);
    return;
  }
  std::move(callback).Run(std::nullopt);
}

void SetCertificate(client_certificates_pb::ClientIdentity& proto_identity,
                    net::X509Certificate& certificate) {
  base::Pickle pickle;
  certificate.Persist(&pickle);
  *proto_identity.mutable_certificate() =
      std::string(pickle.data_as_char(), pickle.size());
}

}  // namespace

// static
std::unique_ptr<LevelDbCertificateStore> LevelDbCertificateStore::Create(
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

  return std::make_unique<LevelDbCertificateStore>(std::move(database),
                                                   std::move(key_factory));
}

// static
std::unique_ptr<LevelDbCertificateStore>
LevelDbCertificateStore::CreateForTesting(
    std::unique_ptr<leveldb_proto::ProtoDatabase<
        client_certificates_pb::ClientIdentity>> database,
    std::unique_ptr<PrivateKeyFactory> key_factory) {
  return std::make_unique<LevelDbCertificateStore>(std::move(database),
                                                   std::move(key_factory));
}

LevelDbCertificateStore::LevelDbCertificateStore(
    std::unique_ptr<leveldb_proto::ProtoDatabase<
        client_certificates_pb::ClientIdentity>> database,
    std::unique_ptr<PrivateKeyFactory> key_factory)
    : database_(std::move(database)), key_factory_(std::move(key_factory)) {
  CHECK(database_);
  CHECK(key_factory_);

  InitializeDatabase();
}

LevelDbCertificateStore::~LevelDbCertificateStore() = default;

void LevelDbCertificateStore::CreatePrivateKey(
    const std::string& identity_name,
    base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)>
        callback) {
  WaitForInitializationAndGetIdentityProto(
      identity_name,
      base::BindOnce(&LevelDbCertificateStore::CreatePrivateKeyInner,
                     weak_factory_.GetWeakPtr(), identity_name,
                     std::move(callback)));
}

void LevelDbCertificateStore::CommitCertificate(
    const std::string& identity_name,
    scoped_refptr<net::X509Certificate> certificate,
    base::OnceCallback<void(std::optional<StoreError>)> callback) {
  WaitForInitializationAndGetIdentityProto(
      identity_name,
      base::BindOnce(&LevelDbCertificateStore::CommitCertificateInner,
                     weak_factory_.GetWeakPtr(), identity_name,
                     std::move(certificate), std::move(callback)));
}

void LevelDbCertificateStore::CommitIdentity(
    const std::string& temporary_identity_name,
    const std::string& final_identity_name,
    scoped_refptr<net::X509Certificate> certificate,
    base::OnceCallback<void(std::optional<StoreError>)> callback) {
  WaitForInitializationAndGetIdentityProto(
      temporary_identity_name,
      base::BindOnce(&LevelDbCertificateStore::CommitIdentityInner,
                     weak_factory_.GetWeakPtr(), temporary_identity_name,
                     final_identity_name, std::move(certificate),
                     std::move(callback)));
}

void LevelDbCertificateStore::GetIdentity(
    const std::string& identity_name,
    base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>
        callback) {
  WaitForInitializationAndGetIdentityProto(
      identity_name, base::BindOnce(&LevelDbCertificateStore::GetIdentityInner,
                                    weak_factory_.GetWeakPtr(), identity_name,
                                    std::move(callback)));
}

void LevelDbCertificateStore::InitializeDatabase(bool retry_on_failure) {
  if (database_state_ != DatabaseState::kUninitialized) {
    return;
  }

  database_state_ = DatabaseState::kInitializing;
  database_->Init(
      base::BindOnce(&LevelDbCertificateStore::OnDatabaseInitialized,
                     weak_factory_.GetWeakPtr(), retry_on_failure));
}

void LevelDbCertificateStore::OnDatabaseInitialized(
    bool retry_on_failure,
    leveldb_proto::Enums::InitStatus status) {
  database_state_ = status == leveldb_proto::Enums::InitStatus::kOK
                        ? DatabaseState::kInitialized
                        : DatabaseState::kUninitialized;

  // Log the status. `retry_on_failure` is only true for the first call.
  LogLevelDBInitStatus(status, /*with_retry=*/!retry_on_failure);

  if (retry_on_failure && database_state_ == DatabaseState::kUninitialized) {
    // Retry failed DB initialization at least once.
    InitializeDatabase(/*retry_on_failure=*/false);
    return;
  }

  for (auto& operation : pending_operations_) {
    std::move(operation).Run();
  }

  pending_operations_.clear();
}

void LevelDbCertificateStore::WaitForInitializationAndGetIdentityProto(
    const std::string& identity_name,
    base::OnceCallback<void(
        StoreErrorOr<std::unique_ptr<client_certificates_pb::ClientIdentity>>)>
        callback) {
  auto get_operation = base::BindOnce(
      &LevelDbCertificateStore::GetIdentityProto, weak_factory_.GetWeakPtr(),
      identity_name, std::move(callback));

  if (database_state_ == DatabaseState::kInitialized) {
    std::move(get_operation).Run();
    return;
  }

  pending_operations_.push_back(std::move(get_operation));

  if (database_state_ == DatabaseState::kUninitialized) {
    InitializeDatabase();
  }
}

void LevelDbCertificateStore::GetIdentityProto(
    const std::string& identity_name,
    base::OnceCallback<void(
        StoreErrorOr<std::unique_ptr<client_certificates_pb::ClientIdentity>>)>
        callback) {
  if (identity_name.empty()) {
    std::move(callback).Run(base::unexpected(StoreError::kInvalidIdentityName));
    return;
  }

  if (database_state_ != DatabaseState::kInitialized) {
    std::move(callback).Run(
        base::unexpected(StoreError::kInvalidDatabaseState));
    return;
  }

  database_->GetEntry(identity_name,
                      base::BindOnce(OnIdentityFetched, std::move(callback)));
}

void LevelDbCertificateStore::CreatePrivateKeyInner(
    const std::string& identity_name,
    base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)> callback,
    StoreErrorOr<std::unique_ptr<client_certificates_pb::ClientIdentity>>
        proto_identity) {
  if (!proto_identity.has_value()) {
    std::move(callback).Run(base::unexpected(proto_identity.error()));
    return;
  }

  const auto& local_proto_identity = proto_identity.value();
  if (local_proto_identity && local_proto_identity->has_private_key()) {
    // An identity already exists, this request is therefore treated as a
    // conflict. Only check for the private key, as certificates can be replaced
    // as long as the trusted private key is not lost.
    std::move(callback).Run(base::unexpected(StoreError::kConflictingIdentity));
    return;
  }

  // In order:
  // 1) Create a private key,
  // 2) Convert the private key to its proto format,
  // 3) Save the private key,
  // 4) Reply to the original callback.
  key_factory_->CreatePrivateKey(base::BindOnce(
      &LevelDbCertificateStore::OnPrivateKeyCreated, weak_factory_.GetWeakPtr(),
      identity_name, std::move(callback)));
}

void LevelDbCertificateStore::OnPrivateKeyCreated(
    const std::string& identity_name,
    base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)> callback,
    scoped_refptr<PrivateKey> private_key) {
  if (!private_key) {
    // Failed to create a private key.
    std::move(callback).Run(base::unexpected(StoreError::kCreateKeyFailed));
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
      base::BindOnce(&LevelDbCertificateStore::OnPrivateKeySaved,
                     weak_factory_.GetWeakPtr(), std::move(private_key),
                     std::move(callback)));
}

void LevelDbCertificateStore::OnPrivateKeySaved(
    scoped_refptr<PrivateKey> private_key,
    base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)> callback,
    bool save_success) {
  if (!save_success) {
    // Failed to save the key in the database, so don't return the private key.
    std::move(callback).Run(base::unexpected(StoreError::kSaveKeyFailed));
    return;
  }

  std::move(callback).Run(private_key);
}

void LevelDbCertificateStore::CommitCertificateInner(
    const std::string& identity_name,
    scoped_refptr<net::X509Certificate> certificate,
    base::OnceCallback<void(std::optional<StoreError>)> callback,
    StoreErrorOr<std::unique_ptr<client_certificates_pb::ClientIdentity>>
        proto_identity) {
  if (!proto_identity.has_value()) {
    std::move(callback).Run(proto_identity.error());
    return;
  }

  if (!certificate) {
    std::move(callback).Run(StoreError::kInvalidCertificateInput);
    return;
  }

  auto& local_proto_identity = proto_identity.value();
  if (!local_proto_identity) {
    // If no identity exists, create one. This simply means a public certificate
    // will be stored with no corresponding private key.
    local_proto_identity =
        std::make_unique<client_certificates_pb::ClientIdentity>();
  }

  // Add cert to proto_identity, and save it to DB.
  SetCertificate(*local_proto_identity, *certificate);

  auto entries_to_save = std::make_unique<leveldb_proto::ProtoDatabase<
      client_certificates_pb::ClientIdentity>::KeyEntryVector>();
  entries_to_save->push_back({identity_name, *local_proto_identity});

  auto entries_to_remove = std::make_unique<std::vector<std::string>>();

  database_->UpdateEntries(
      std::move(entries_to_save), std::move(entries_to_remove),
      base::BindOnce(OnCertificateCommitted, std::move(callback)));
}

void LevelDbCertificateStore::CommitIdentityInner(
    const std::string& temporary_identity_name,
    const std::string& final_identity_name,
    scoped_refptr<net::X509Certificate> certificate,
    base::OnceCallback<void(std::optional<StoreError>)> callback,
    StoreErrorOr<std::unique_ptr<client_certificates_pb::ClientIdentity>>
        proto_identity) {
  if (!proto_identity.has_value()) {
    std::move(callback).Run(proto_identity.error());
    return;
  }

  if (!certificate) {
    std::move(callback).Run(StoreError::kInvalidCertificateInput);
    return;
  }

  if (final_identity_name.empty()) {
    std::move(callback).Run(StoreError::kInvalidFinalIdentityName);
    return;
  }

  auto& local_proto_identity = proto_identity.value();
  if (!local_proto_identity) {
    std::move(callback).Run(StoreError::kIdentityNotFound);
    return;
  }

  SetCertificate(*local_proto_identity, *certificate);

  auto entries_to_save = std::make_unique<leveldb_proto::ProtoDatabase<
      client_certificates_pb::ClientIdentity>::KeyEntryVector>();
  entries_to_save->push_back({final_identity_name, *local_proto_identity});

  auto entries_to_remove = std::make_unique<std::vector<std::string>>();
  entries_to_remove->push_back(temporary_identity_name);

  database_->UpdateEntries(
      std::move(entries_to_save), std::move(entries_to_remove),
      base::BindOnce(OnCertificateCommitted, std::move(callback)));
}

void LevelDbCertificateStore::GetIdentityInner(
    const std::string& identity_name,
    base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>
        callback,
    StoreErrorOr<std::unique_ptr<client_certificates_pb::ClientIdentity>>
        proto_identity) {
  if (!proto_identity.has_value()) {
    std::move(callback).Run(base::unexpected(proto_identity.error()));
    return;
  }

  const auto& local_proto_identity = proto_identity.value();
  if (!local_proto_identity) {
    // Since there were no preconditions error, not finding the identity is a
    // valid use-case (none existed before).
    std::move(callback).Run(std::nullopt);
    return;
  }

  scoped_refptr<net::X509Certificate> certificate = nullptr;
  if (local_proto_identity->has_certificate()) {
    base::Pickle pickle = base::Pickle::WithUnownedBuffer(
        base::as_byte_span(local_proto_identity->certificate()));
    base::PickleIterator iter(pickle);
    certificate = net::X509Certificate::CreateFromPickle(&iter);
  }

  if (!local_proto_identity->has_private_key()) {
    std::move(callback).Run(ClientIdentity(
        identity_name, /*private_key=*/nullptr, std::move(certificate)));
    return;
  }

  key_factory_->LoadPrivateKey(
      local_proto_identity->private_key(),
      base::BindOnce(OnKeyLoaded, identity_name, std::move(certificate),
                     std::move(callback)));
}

}  // namespace client_certificates
