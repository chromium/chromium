// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_LEVELDB_CERTIFICATE_STORE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_LEVELDB_CERTIFICATE_STORE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/client_certificates/core/certificate_store.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/store_error.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/leveldb_proto/public/proto_database.h"

namespace client_certificates_pb {
class ClientIdentity;
}

namespace leveldb_proto {
class ProtoDatabaseProvider;
}  // namespace leveldb_proto

namespace net {
class X509Certificate;
}  // namespace net

namespace client_certificates {

class PrivateKeyFactory;
class PrivateKey;

// Implementation of the CertificateStore backed by a
// LevelDB database for storage.
class LevelDbCertificateStore : public CertificateStore {
 public:
  LevelDbCertificateStore(std::unique_ptr<leveldb_proto::ProtoDatabase<
                              client_certificates_pb::ClientIdentity>> database,
                          std::unique_ptr<PrivateKeyFactory> key_factory);

  ~LevelDbCertificateStore() override;

  // Creates a CertificateStore instance where the LevelDB database file is
  // located under `profile_dir` and loaded using `database_provider`.
  // `key_factory` will be used to create and load private keys into memory.
  static std::unique_ptr<LevelDbCertificateStore> Create(
      const base::FilePath& profile_dir,
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      std::unique_ptr<PrivateKeyFactory> key_factory);

  // Creates a CertificateStore instance with the given `database` and
  // `key_factory` instances. To be used to testing only.
  static std::unique_ptr<LevelDbCertificateStore> CreateForTesting(
      std::unique_ptr<leveldb_proto::ProtoDatabase<
          client_certificates_pb::ClientIdentity>> database,
      std::unique_ptr<PrivateKeyFactory> key_factory);

  // CertificateStore:
  void CreatePrivateKey(
      const std::string& identity_name,
      base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)>
          callback) override;
  void CommitCertificate(
      const std::string& identity_name,
      scoped_refptr<net::X509Certificate> certificate,
      base::OnceCallback<void(std::optional<StoreError>)> callback) override;
  void CommitIdentity(
      const std::string& temporary_identity_name,
      const std::string& final_identity_name,
      scoped_refptr<net::X509Certificate> certificate,
      base::OnceCallback<void(std::optional<StoreError>)> callback) override;
  void GetIdentity(
      const std::string& identity_name,
      base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>
          callback) override;

 private:
  enum class DatabaseState {
    kUninitialized = 0,
    kInitializing = 1,
    kInitialized = 2,
  };

  // Will start the initialization of the Database. Is a no-op is the database
  // is already initialized.
  void InitializeDatabase(bool retry_on_failure = true);

  // Invoked as callback when the database is done initializing with `status` as
  // result.
  void OnDatabaseInitialized(bool retry_on_failure,
                             leveldb_proto::Enums::InitStatus status);

  // Will wait for the database to be initialized and then retrieve the entry
  // with `identity_name`. If successful, will invoke `callback` with
  // std::nullopt and the entry. If unsuccessful, will invoke with the error and
  // nullptr.
  void WaitForInitializationAndGetIdentityProto(
      const std::string& identity_name,
      base::OnceCallback<void(StoreErrorOr<std::unique_ptr<
                                  client_certificates_pb::ClientIdentity>>)>
          callback);

  // Will get the proto ClientIdentity object stored in the database with key
  // `identity_name` and return it via `callback;
  void GetIdentityProto(
      const std::string& identity_name,
      base::OnceCallback<void(StoreErrorOr<std::unique_ptr<
                                  client_certificates_pb::ClientIdentity>>)>
          callback);

  // Inner functions of the main interface functions, which contains the main
  // business logic and can be called asynchronously after certain conditions
  // are met (e.g. database initialization).
  void CreatePrivateKeyInner(
      const std::string& identity_name,
      base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)>
          callback,
      StoreErrorOr<std::unique_ptr<client_certificates_pb::ClientIdentity>>
          proto_identity);
  void CommitCertificateInner(
      const std::string& identity_name,
      scoped_refptr<net::X509Certificate> certificate,
      base::OnceCallback<void(std::optional<StoreError>)> callback,
      StoreErrorOr<std::unique_ptr<client_certificates_pb::ClientIdentity>>
          proto_identity);
  void CommitIdentityInner(
      const std::string& temporary_identity_name,
      const std::string& final_identity_name,
      scoped_refptr<net::X509Certificate> certificate,
      base::OnceCallback<void(std::optional<StoreError>)> callback,
      StoreErrorOr<std::unique_ptr<client_certificates_pb::ClientIdentity>>
          proto_identity);
  void GetIdentityInner(
      const std::string& identity_name,
      base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>
          callback,
      StoreErrorOr<std::unique_ptr<client_certificates_pb::ClientIdentity>>
          proto_identity);

  // Called when a private key was created from `CreatePrivateKeyInner`.
  // `private_key` will contain the created private key when successful, or
  // nullptr when not.
  void OnPrivateKeyCreated(
      const std::string& identity_name,
      base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)>
          callback,
      scoped_refptr<PrivateKey> private_key);

  // Called when a private key was saved into the database from
  // `OnPrivateKeyCreated`. `save_success` indicates whether the database's
  // update was successful or not.
  void OnPrivateKeySaved(
      scoped_refptr<PrivateKey> private_key,
      base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)>
          callback,
      bool save_success);

  std::vector<base::OnceClosure> pending_operations_;

  std::unique_ptr<
      leveldb_proto::ProtoDatabase<client_certificates_pb::ClientIdentity>>
      database_;
  std::unique_ptr<PrivateKeyFactory> key_factory_;
  DatabaseState database_state_{DatabaseState::kUninitialized};

  base::WeakPtrFactory<LevelDbCertificateStore> weak_factory_{this};
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_LEVELDB_CERTIFICATE_STORE_H_
