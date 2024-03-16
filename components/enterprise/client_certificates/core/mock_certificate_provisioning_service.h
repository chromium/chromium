// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_CERTIFICATE_PROVISIONING_SERVICE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_CERTIFICATE_PROVISIONING_SERVICE_H_

#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/core/certificate_store.h"
#include "components/enterprise/client_certificates/core/key_upload_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace client_certificates {

class MockCertificateProvisioningService
    : public CertificateProvisioningService {
 public:
  MockCertificateProvisioningService();
  ~MockCertificateProvisioningService() override;

  MOCK_METHOD(void,
              GetManagedIdentity,
              (GetManagedIdentityCallback),
              (override));
  MOCK_METHOD(Status, GetCurrentStatus, (), (const, override));
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_MOCK_CERTIFICATE_PROVISIONING_SERVICE_H_
