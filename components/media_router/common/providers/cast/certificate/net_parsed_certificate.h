// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_NET_PARSED_CERTIFICATE_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_NET_PARSED_CERTIFICATE_H_

#include "third_party/boringssl/src/pki/parsed_certificate.h"
#include "third_party/openscreen/src/cast/common/public/parsed_certificate.h"

namespace cast_certificate {

bssl::ParseCertificateOptions GetCertParsingOptions();

class NetParsedCertificate final : public openscreen::cast::ParsedCertificate {
 public:
  explicit NetParsedCertificate(
      std::shared_ptr<const bssl::ParsedCertificate> cert);
  ~NetParsedCertificate() override;

  // openscreen::cast::ParsedCertificate implementation:
  openscreen::ErrorOr<std::vector<uint8_t>> SerializeToDER(
      int front_spacing) const override;

  openscreen::ErrorOr<openscreen::cast::DateTime> GetNotBeforeTime()
      const override;
  openscreen::ErrorOr<openscreen::cast::DateTime> GetNotAfterTime()
      const override;

  std::string GetCommonName() const override;

  std::string GetSpkiTlv() const override;

  openscreen::ErrorOr<uint64_t> GetSerialNumber() const override;

  bool VerifySignedData(openscreen::cast::DigestAlgorithm algorithm,
                        const openscreen::ByteView& data,
                        const openscreen::ByteView& signature) const override;

  bool HasPolicyOid(const openscreen::ByteView& oid) const override;

  void SetNotBeforeTimeForTesting(time_t not_before) override;
  void SetNotAfterTimeForTesting(time_t not_after) override;

 private:
  std::shared_ptr<const bssl::ParsedCertificate> cert_;
};

}  // namespace cast_certificate

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CERTIFICATE_NET_PARSED_CERTIFICATE_H_
