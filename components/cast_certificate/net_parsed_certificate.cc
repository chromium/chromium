// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cast_certificate/net_parsed_certificate.h"

#include "base/containers/contains.h"
#include "net/cert/internal/parse_name.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/cert/internal/verify_signed_data.h"
#include "net/cert/x509_util.h"
#include "net/der/encode_values.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"

namespace openscreen::cast {

// static
ErrorOr<std::unique_ptr<ParsedCertificate>> ParsedCertificate::ParseFromDER(
    const std::vector<uint8_t>& der_cert) {
  scoped_refptr<net::ParsedCertificate> cert = net::ParsedCertificate::Create(
      net::x509_util::CreateCryptoBuffer(base::span<const uint8_t>(der_cert)),
      cast_certificate::GetCertParsingOptions(), nullptr);
  if (!cert) {
    return Error::Code::kErrCertsParse;
  }
  std::unique_ptr<ParsedCertificate> result =
      std::make_unique<::cast_certificate::NetParsedCertificate>(
          std::move(cert));
  return result;
}

}  // namespace openscreen::cast

namespace cast_certificate {

using openscreen::ErrorOr;
using openscreen::cast::ConstDataSpan;
using openscreen::cast::DateTime;
using openscreen::cast::DigestAlgorithm;

namespace {

// Helper that extracts the Common Name from a certificate's subject field. On
// success |common_name| contains the text for the attribute (UTF-8, but for
// Cast device certs it should be ASCII).
bool GetCommonNameFromSubject(const net::der::Input& subject_tlv,
                              std::string* common_name) {
  net::RDNSequence rdn_sequence;
  if (!net::ParseName(subject_tlv, &rdn_sequence))
    return false;

  for (const net::RelativeDistinguishedName& relative_distinguished_name :
       rdn_sequence) {
    for (const auto& attribute_type_value : relative_distinguished_name) {
      if (attribute_type_value.type ==
          net::der::Input(net::kTypeCommonNameOid)) {
        return attribute_type_value.ValueAsString(common_name);
      }
    }
  }
  return false;
}

}  // namespace

net::ParseCertificateOptions GetCertParsingOptions() {
  net::ParseCertificateOptions options;

  // Some cast intermediate certificates contain serial numbers that are
  // 21 octets long, and might also not use valid DER encoding for an
  // INTEGER (non-minimal encoding).
  //
  // Allow these sorts of serial numbers.
  //
  // TODO(eroman): At some point in the future this workaround will no longer be
  // necessary. Should revisit this for removal in 2017 if not earlier.  We will
  // probably want an UMA histogram to be certain.
  options.allow_invalid_serial_numbers = true;
  return options;
}

NetParsedCertificate::NetParsedCertificate(
    scoped_refptr<net::ParsedCertificate> cert)
    : cert_(std::move(cert)) {}

NetParsedCertificate::~NetParsedCertificate() = default;

ErrorOr<std::vector<uint8_t>> NetParsedCertificate::SerializeToDER(
    int front_spacing) const {
  std::vector<uint8_t> result;
  base::span<const uint8_t> der_buffer = cert_->der_cert().AsSpan();
  result.reserve(front_spacing + der_buffer.size());
  result.resize(front_spacing);
  result.insert(result.end(), der_buffer.begin(), der_buffer.end());
  return result;
}

ErrorOr<DateTime> NetParsedCertificate::GetNotBeforeTime() const {
  const net::der::GeneralizedTime& t = cert_->tbs().validity_not_before;
  DateTime result;
  result.year = t.year;
  result.month = t.month;
  result.day = t.day;
  result.hour = t.hours;
  result.minute = t.minutes;
  result.second = t.seconds;
  return result;
}

ErrorOr<DateTime> NetParsedCertificate::GetNotAfterTime() const {
  const net::der::GeneralizedTime& t = cert_->tbs().validity_not_after;
  DateTime result;
  result.year = t.year;
  result.month = t.month;
  result.day = t.day;
  result.hour = t.hours;
  result.minute = t.minutes;
  result.second = t.seconds;
  return result;
}

std::string NetParsedCertificate::GetCommonName() const {
  std::string common_name;
  if (!cert_ ||
      !GetCommonNameFromSubject(cert_->tbs().subject_tlv, &common_name)) {
    return {};
  }
  return common_name;
}

std::string NetParsedCertificate::GetSpkiTlv() const {
  return cert_->tbs().spki_tlv.AsString();
}

ErrorOr<uint64_t> NetParsedCertificate::GetSerialNumber() const {
  uint64_t serial_number;
  if (!net::der::ParseUint64(cert_->tbs().serial_number, &serial_number)) {
    return openscreen::Error::Code::kErrCertsParse;
  }
  return serial_number;
}

bool NetParsedCertificate::VerifySignedData(
    DigestAlgorithm algorithm,
    const ConstDataSpan& data,
    const ConstDataSpan& signature) const {
  base::StringPiece signature_piece(
      reinterpret_cast<const char*>(signature.data), signature.length);
  base::StringPiece data_piece(reinterpret_cast<const char*>(data.data),
                               data.length);

  net::DigestAlgorithm net_digest = net::DigestAlgorithm::Sha1;
  switch (algorithm) {
    case DigestAlgorithm::kSha1:
      net_digest = net::DigestAlgorithm::Sha1;
      break;
    case DigestAlgorithm::kSha256:
      net_digest = net::DigestAlgorithm::Sha256;
      break;
    case DigestAlgorithm::kSha384:
      net_digest = net::DigestAlgorithm::Sha384;
      break;
    case DigestAlgorithm::kSha512:
      net_digest = net::DigestAlgorithm::Sha512;
      break;
    default:
      return false;
  }
  // This code assumes the signature algorithm was RSASSA PKCS#1 v1.5 with
  // |digest_algorithm|.
  auto signature_algorithm =
      net::SignatureAlgorithm::CreateRsaPkcs1(net_digest);

  return net::VerifySignedData(
      *signature_algorithm, net::der::Input(data_piece),
      net::der::BitString(net::der::Input(signature_piece), 0),
      cert_->tbs().spki_tlv);
}

bool NetParsedCertificate::HasPolicyOid(const ConstDataSpan& oid) const {
  net::der::Input net_oid(oid.data, oid.length);
  if (!cert_->has_policy_oids()) {
    return false;
  }
  const std::vector<net::der::Input>& policies = cert_->policy_oids();
  return base::Contains(policies, net_oid);
}

void NetParsedCertificate::SetNotBeforeTimeForTesting(time_t not_before) {
  CHECK(net::der::EncodeTimeAsGeneralizedTime(
      base::Time::FromTimeT(not_before),
      const_cast<net::der::GeneralizedTime*>(
          &cert_->tbs().validity_not_before)));
}

void NetParsedCertificate::SetNotAfterTimeForTesting(time_t not_after) {
  CHECK(net::der::EncodeTimeAsGeneralizedTime(
      base::Time::FromTimeT(not_after), const_cast<net::der::GeneralizedTime*>(
                                            &cert_->tbs().validity_not_after)));
}

}  // namespace cast_certificate
