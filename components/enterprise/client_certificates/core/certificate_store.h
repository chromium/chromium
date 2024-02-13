// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CERTIFICATE_STORE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CERTIFICATE_STORE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
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

// Class representing a store backed by a LevelDB database which facilitates the
// creation and storage of private keys, and storage of certificates along with
// a private key.
class CertificateStore : public KeyedService {
 public:
  CertificateStore();
  ~CertificateStore() override;

  // Creates a CertificateStore instance where the LevelDB database file is
  // located under `profile_dir` and loaded using `database_provider`.
  // `key_factory` will be used to create and load private keys into memory.
  static std::unique_ptr<CertificateStore> Create(
      const base::FilePath& profile_dir,
      leveldb_proto::ProtoDatabaseProvider* database_provider,
      std::unique_ptr<PrivateKeyFactory> key_factory);

  // Creates a CertificateStore instance with the given `database` and
  // `key_factory` instances. To be used to testing only.
  static std::unique_ptr<CertificateStore> CreateForTesting(
      std::unique_ptr<leveldb_proto::ProtoDatabase<
          client_certificates_pb::ClientIdentity>> database,
      std::unique_ptr<PrivateKeyFactory> key_factory);

  // Will create a private key with the strongest protection available on the
  // device and store it in the database under `identity_name`. If successful,
  // `callback` will be invoked with the key once it has been created and
  // stored. If an error occurred, `callback` will be invoked with the error.
  virtual void CreatePrivateKey(
      const std::string& identity_name,
      base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)>
          callback) = 0;

  // Will store the given `certificate` in the database under `identity_name`.
  // Will call `callback` with std::nullopt if successful, or an error if not.
  // This API can be used to update an expired certificate without changing
  // the existing private key.
  virtual void CommitCertificate(
      const std::string& identity_name,
      scoped_refptr<net::X509Certificate> certificate,
      base::OnceCallback<void(std::optional<StoreError>)> callback) = 0;

  // Will move the private key with name `temporary_identity_name` and move
  // it into `final_identity_name` alongside the given `certificate`.
  // Will overwrite any pre-existing identity stored under
  // `final_identity_name`. Will call `callback` with std::nullopt if
  // successful, or an error if not. This API can be used as the last step in
  // committing an updated identity when the private key needed to be rotated.
  virtual void CommitIdentity(
      const std::string& temporary_identity_name,
      const std::string& final_identity_name,
      scoped_refptr<net::X509Certificate> certificate,
      base::OnceCallback<void(std::optional<StoreError>)> callback) = 0;

  // Will attempt to retrieve an identity with name `identity_name` from the
  // database and load it into memory. Will return the loaded identity via
  // `callback`. Will invoke `callback` with std::nullopt if the identity
  // doesn't exist, or with an error if something went wrong.
  virtual void GetIdentity(
      const std::string& identity_name,
      base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>
          callback) = 0;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CERTIFICATE_STORE_H_
