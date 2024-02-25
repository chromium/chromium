// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_CERTIFICATE_STORE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_CERTIFICATE_STORE_H_

#include "base/functional/callback.h"
#include "components/enterprise/client_certificates/core/certificate_store.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "net/cert/x509_certificate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace client_certificates {

class MockCertificateStore : public CertificateStore {
 public:
  MockCertificateStore();
  ~MockCertificateStore() override;

  MOCK_METHOD(
      void,
      CreatePrivateKey,
      (const std::string&,
       base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)>),
      (override));
  MOCK_METHOD(void,
              CommitCertificate,
              (const std::string&,
               scoped_refptr<net::X509Certificate>,
               base::OnceCallback<void(std::optional<StoreError>)>),
              (override));
  MOCK_METHOD(void,
              CommitIdentity,
              (const std::string&,
               const std::string&,
               scoped_refptr<net::X509Certificate>,
               base::OnceCallback<void(std::optional<StoreError>)>),
              (override));
  MOCK_METHOD(
      void,
      GetIdentity,
      (const std::string&,
       base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>),
      (override));
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_CERTIFICATE_STORE_H_
