// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_IOS_CERTIFICATE_PROVISIONING_SERVICE_IOS_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_IOS_CERTIFICATE_PROVISIONING_SERVICE_IOS_H_

#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "components/enterprise/client_certificates/ios/client_identity_ios.h"

namespace client_certificates {

// This class extends the CertificateProvisioningService to expose an
// iOS-specific version of ClientIdentity which provides access to a
// SecIdentityRef.
class CertificateProvisioningServiceIOS
    : public CertificateProvisioningService {
 public:
  using GetManagedIdentityIOSCallback =
      base::OnceCallback<void(std::optional<ClientIdentityIOS>)>;

  ~CertificateProvisioningServiceIOS() override;

  static std::unique_ptr<CertificateProvisioningServiceIOS> Create(
      std::unique_ptr<CertificateProvisioningService> core_service);

  // iOS-specific method to retrieve the identity with the `SecIdentityRef`.
  virtual void GetManagedIdentityIOS(
      GetManagedIdentityIOSCallback callback) = 0;
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_IOS_CERTIFICATE_PROVISIONING_SERVICE_IOS_H_
