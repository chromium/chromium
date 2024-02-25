// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_PRIVATE_KEY_FACTORY_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_PRIVATE_KEY_FACTORY_H_

#include "base/functional/callback.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"
#include "components/enterprise/client_certificates/proto/client_certificates_database.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace client_certificates {

class MockPrivateKeyFactory : public PrivateKeyFactory {
 public:
  MockPrivateKeyFactory();
  ~MockPrivateKeyFactory() override;

  MOCK_METHOD(void,
              CreatePrivateKey,
              (PrivateKeyFactory::PrivateKeyCallback),
              (override));
  MOCK_METHOD(void,
              LoadPrivateKey,
              (const client_certificates_pb::PrivateKey&,
               PrivateKeyFactory::PrivateKeyCallback),
              (override));
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_PRIVATE_KEY_FACTORY_H_
