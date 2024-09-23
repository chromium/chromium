// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_NET_TRUST_STORE_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_NET_TRUST_STORE_H_

#include <vector>

#include "base/check.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"
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
    AddAnchor(base::span<const uint8_t>(data, N));
  }

  void AddAnchor(base::span<const uint8_t> data);

  openscreen::ErrorOr<CertificatePathResult> FindCertificatePath(
      const std::vector<std::string>& der_certs,
      const openscreen::cast::DateTime& time) override;

 private:
  bssl::TrustStoreInMemory store_;
};

}  // namespace cast_certificate

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_NET_TRUST_STORE_H_
