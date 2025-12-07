// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_CONTEXT_DELEGATE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_CONTEXT_DELEGATE_H_

#include "components/enterprise/client_certificates/core/context_delegate.h"
#include "net/cert/x509_certificate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace client_certificates {

class MockContextDelegate : public ContextDelegate {
 public:
  MockContextDelegate();
  ~MockContextDelegate() override;

  // ContextDelegate:
  MOCK_METHOD(void,
              OnClientCertificateDeleted,
              (scoped_refptr<net::X509Certificate>),
              (override));
  MOCK_METHOD(std::string, GetIdentityName, (), (override));
  MOCK_METHOD(std::string, GetTemporaryIdentityName, (), (override));
  MOCK_METHOD(std::string, GetPolicyPref, (), (override));
  MOCK_METHOD(std::string, GetLoggingContext, (), (override));
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_CONTEXT_DELEGATE_H_
