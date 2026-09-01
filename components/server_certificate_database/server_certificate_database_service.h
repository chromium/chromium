// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_SERVICE_H_
#define COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_SERVICE_H_

#include <vector>

#include "base/callback_list.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/server_certificate_database/server_certificate_database.h"

namespace net {

// KeyedService that loads and provides policies around usage of Certificates
// for TLS.
class COMPONENT_EXPORT(SERVER_CERTIFICATE_DATABASE)
    ServerCertificateDatabaseService : public KeyedService {
 public:

  using GetCertificatesCallback = base::OnceCallback<void(
      std::vector<net::ServerCertificateDatabase::CertInformation>)>;

  explicit ServerCertificateDatabaseService(base::FilePath profile_path);

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

  void GetCertificatesCount(base::OnceCallback<void(uint32_t)> callback);

  void DeleteCertificate(const std::string& sha256hash_hex,
                         base::OnceCallback<void(bool)> callback);

  base::WeakPtr<ServerCertificateDatabaseService> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void HandleModificationResult(base::OnceCallback<void(bool)> callback,
                                bool success);

  const base::FilePath profile_path_;

  base::SequenceBound<net::ServerCertificateDatabase> server_cert_database_;

  base::RepeatingClosureList observers_;

  base::WeakPtrFactory<ServerCertificateDatabaseService> weak_factory_{this};
};

}  // namespace net

#endif  // COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_SERVICE_H_
