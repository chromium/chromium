// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_PRIVATE_KEY_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_PRIVATE_KEY_H_

#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/proto/client_certificates_database.pb.h"
#include "net/ssl/ssl_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace client_certificates {

class MockPrivateKey : public PrivateKey {
 public:
  explicit MockPrivateKey(
      PrivateKeySource source = PrivateKeySource::kUnexportableKey,
      scoped_refptr<net::SSLPrivateKey> ssl_private_key = nullptr);

  MOCK_METHOD(std::optional<std::vector<uint8_t>>,
              SignSlowly,
              (base::span<const uint8_t>),
              (const, override));
  MOCK_METHOD(std::vector<uint8_t>,
              GetSubjectPublicKeyInfo,
              (),
              (const, override));
  MOCK_METHOD(crypto::SignatureVerifier::SignatureAlgorithm,
              GetAlgorithm,
              (),
              (const, override));
  MOCK_METHOD(client_certificates_pb::PrivateKey,
              ToProto,
              (),
              (const, override));

 protected:
  ~MockPrivateKey() override;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_PRIVATE_KEY_H_
