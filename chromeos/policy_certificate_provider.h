// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_POLICY_CERTIFICATE_PROVIDER_H_
#define CHROMEOS_POLICY_CERTIFICATE_PROVIDER_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"

namespace net {
class X509Certificate;
using CertificateList = std::vector<scoped_refptr<X509Certificate>>;
}  // namespace net

namespace chromeos {

// An interface for a class which makes server and authority certificates
// available from enterprise policy. Clients of this interface can register as
// |Observer|s to receive update notifications.
class PolicyCertificateProvider {
 public:
  virtual ~PolicyCertificateProvider() {}

  class Observer {
   public:
    virtual ~Observer() = default;

    // Is called every time the list of policy-set server and authority
    // certificates changes.
    virtual void OnPolicyProvidedCertsChanged(
        const net::CertificateList& all_server_and_authority_certs,
        const net::CertificateList& trust_anchors) = 0;
  };

  virtual void AddPolicyProvidedCertsObserver(Observer* observer) = 0;
  virtual void RemovePolicyProvidedCertsObserver(Observer* observer) = 0;

  // Returns all server and authority certificates successfully parsed from ONC,
  // independent of their trust bits.
  virtual net::CertificateList GetAllServerAndAuthorityCertificates() const = 0;

  // Returns all authority certificates successfully parsed from ONC,
  // independent of their trust bits.
  virtual net::CertificateList GetAllAuthorityCertificates() const = 0;

  // Returns the server and authority certificates which were successfully
  // parsed from ONC and were granted web trust. This means that the
  // certificates had the "Web" trust bit set, and this
  // UserNetworkConfigurationUpdater instance was created with
  // |allow_trusted_certs_from_policy| = true.
  virtual net::CertificateList GetWebTrustedCertificates() const = 0;

  // Returns the server and authority certificates which were successfully
  // parsed from ONC and did not request or were not granted web trust.
  // This is equivalent to calling |GetAllServerAndAuthorityCertificates| and
  // then removing all certificates returned by |GetWebTrustedCertificates| from
  // the result.
  virtual net::CertificateList GetCertificatesWithoutWebTrust() const = 0;
};

}  // namespace chromeos

#endif  // CHROMEOS_POLICY_CERTIFICATE_PROVIDER_H_
