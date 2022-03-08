// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CERTIFICATE_NET_TRUST_STORE_H_
#define COMPONENTS_CAST_CERTIFICATE_NET_TRUST_STORE_H_

#include <vector>

#include "base/check.h"
#include "net/cert/internal/cert_errors.h"
#include "net/cert/internal/trust_store_in_memory.h"
#include "net/cert/x509_util.h"
#include "third_party/openscreen/src/cast/common/public/trust_store.h"

namespace cast_certificate {

class NetTrustStore final : public openscreen::cast::TrustStore {
 public:
  using openscreen::cast::TrustStore::CertificatePathResult;

  NetTrustStore();
  ~NetTrustStore() override;

  // Adds a trust anchor given a DER-encoded certificate from static storage.
  template <size_t N>
  void AddAnchor(const uint8_t (&data)[N]) {
    net::CertErrors errors;
    scoped_refptr<net::ParsedCertificate> cert = net::ParsedCertificate::Create(
        net::x509_util::CreateCryptoBuffer(base::span<const uint8_t>(data, N)),
        {}, &errors);
    CHECK(cert) << errors.ToDebugString();
    // Enforce pathlen constraints and policies defined on the root certificate.
    store_.AddTrustAnchorWithConstraints(std::move(cert));
  }

  openscreen::ErrorOr<CertificatePathResult> FindCertificatePath(
      const std::vector<std::string>& der_certs,
      const openscreen::cast::DateTime& time) override;

 private:
  net::TrustStoreInMemory store_;
};

}  // namespace cast_certificate

#endif  // COMPONENTS_CAST_CERTIFICATE_NET_TRUST_STORE_H_
