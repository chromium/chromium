// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CLIENT_CERTIFICATES_SERVICE_H_
#define COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CLIENT_CERTIFICATES_SERVICE_H_

#include "net/ssl/client_cert_store.h"

namespace client_certificates {

class CertificateProvisioningService;

// Service used as aggregator of client certificate stores of various levels
// (e.g. platform, user, browser).
class ClientCertificatesService : public net::ClientCertStore {
 public:
  // Returns an instance of the service which will aggregate certificates from
  // both the managed `certificate_provisioning_service` and the OS'
  // `platform_certificate_store`.
  static std::unique_ptr<ClientCertificatesService> Create(
      CertificateProvisioningService* certificate_provisioning_service,
      std::unique_ptr<net::ClientCertStore> platform_certificate_store);
};

}  // namespace client_certificates

#endif  // COMPONENTS_ENTERPRISE_CLIENT_CERTIFICATES_CORE_CLIENT_CERTIFICATES_SERVICE_H_
