// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PREFS_CERTIFICATE_STORE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PREFS_CERTIFICATE_STORE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/client_certificates/core/certificate_store.h"
#include "components/enterprise/client_certificates/core/store_error.h"

class PrefService;

namespace client_certificates_pb {
class ClientIdentity;
}

namespace net {
class X509Certificate;
}  // namespace net

namespace client_certificates {

class PrivateKeyFactory;
class PrivateKey;

// Implementation of the CertificateStore backed by a
// the prefs service for storage.
class PrefsCertificateStore : public CertificateStore {
 public:
  PrefsCertificateStore(PrefService* pref_service,
                        std::unique_ptr<PrivateKeyFactory> key_factory);

  ~PrefsCertificateStore() override;

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
  // Called when a private key was created from `CreatePrivateKeyInner`.
  // `private_key` will contain the created private key when successful, or
  // nullptr when not.
  void OnPrivateKeyCreated(
      const std::string& identity_name,
      base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)>
          callback,
      scoped_refptr<PrivateKey> private_key);

  raw_ptr<PrefService> pref_service_;
  std::unique_ptr<PrivateKeyFactory> key_factory_;

  base::WeakPtrFactory<PrefsCertificateStore> weak_factory_{this};
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_PREFS_CERTIFICATE_STORE_H_
