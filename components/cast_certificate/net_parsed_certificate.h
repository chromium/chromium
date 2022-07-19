// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CAST_CERTIFICATE_NET_PARSED_CERTIFICATE_H_
#define COMPONENTS_CAST_CERTIFICATE_NET_PARSED_CERTIFICATE_H_

#include "net/cert/pki/parsed_certificate.h"
#include "third_party/openscreen/src/cast/common/public/parsed_certificate.h"

namespace cast_certificate {

net::ParseCertificateOptions GetCertParsingOptions();

class NetParsedCertificate final : public openscreen::cast::ParsedCertificate {
 public:
  explicit NetParsedCertificate(scoped_refptr<net::ParsedCertificate> cert);
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

  bool VerifySignedData(
      openscreen::cast::DigestAlgorithm algorithm,
      const openscreen::cast::ConstDataSpan& data,
      const openscreen::cast::ConstDataSpan& signature) const override;

  bool HasPolicyOid(const openscreen::cast::ConstDataSpan& oid) const override;

  void SetNotBeforeTimeForTesting(time_t not_before) override;
  void SetNotAfterTimeForTesting(time_t not_after) override;

 private:
  scoped_refptr<net::ParsedCertificate> cert_;
};

}  // namespace cast_certificate

#endif  // COMPONENTS_CAST_CERTIFICATE_NET_PARSED_CERTIFICATE_H_
