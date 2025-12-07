// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_H_
#define CHROMEOS_COMPONENTS_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_H_

#include "base/component_export.h"
#include "net/ssl/client_cert_identity.h"

namespace chromeos {
namespace certificate_provider {

class COMPONENT_EXPORT(CERTIFICATE_PROVIDER) CertificateProvider {
 public:
  CertificateProvider() = default;
  CertificateProvider(const CertificateProvider&) = delete;
  CertificateProvider& operator=(const CertificateProvider&) = delete;
  virtual ~CertificateProvider() = default;

  virtual void GetCertificates(
      base::OnceCallback<void(net::ClientCertIdentityList)> callback) = 0;
};

}  // namespace certificate_provider
}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CERTIFICATE_PROVIDER_CERTIFICATE_PROVIDER_H_
