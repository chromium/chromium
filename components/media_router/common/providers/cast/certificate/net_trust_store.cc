// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/certificate/net_trust_store.h"

#include <string_view>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "components/media_router/common/providers/cast/certificate/net_parsed_certificate.h"
#include "net/cert/pem.h"
#include "net/cert/pki/cert_issuer_source_static.h"
#include "net/cert/pki/common_cert_errors.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/pki/path_builder.h"
#include "net/cert/pki/simple_path_builder_delegate.h"
#include "net/cert/x509_util.h"
#include "third_party/openscreen/src/cast/common/public/trust_store.h"

namespace {

// -------------------------------------------------------------------------
// Cast trust anchors.
// -------------------------------------------------------------------------

// There are two trusted roots for Cast certificate chains:
//
//   (1) CN=Cast Root CA    (kCastRootCaDer)
//   (2) CN=Eureka Root CA  (kEurekaRootCaDer)
//
// These constants are defined by the files included next:

#include "components/media_router/common/providers/cast/certificate/cast_root_ca_cert_der-inc.h"
#include "components/media_router/common/providers/cast/certificate/eureka_root_ca_der-inc.h"

// -------------------------------------------------------------------------
// Cast CRL trust anchors.
// -------------------------------------------------------------------------

// There is one trusted root for Cast CRL certificate chains:
//
//   (1) CN=Cast CRL Root CA    (kCastCRLRootCaDer)
//
// These constants are defined by the file included next:

#include "components/media_router/common/providers/cast/certificate/cast_crl_root_ca_cert_der-inc.h"

}  // namespace

namespace openscreen::cast {

// static
std::unique_ptr<openscreen::cast::TrustStore> TrustStore::CreateInstanceForTest(
    const std::vector<uint8_t>& trust_anchor_der) {
  // TODO(issuetracker.google.com/222145200): We need to allow linking this
  // implementation into `openscreen_unittests` when in the chromium waterfall.
  auto result = std::make_unique<cast_certificate::NetTrustStore>();
  result->AddAnchor(trust_anchor_der);
  return result;
}

// static
std::unique_ptr<openscreen::cast::TrustStore>
TrustStore::CreateInstanceFromPemFile(std::string_view file_path) {
  std::string pem_data;
  CHECK(base::ReadFileToString(base::FilePath::FromASCII(base::StringPiece(
                                   file_path.data(), file_path.size())),
                               &pem_data));
  net::PEMTokenizer tokenizer(pem_data, {std::string("CERTIFICATE")});
  auto result = std::make_unique<cast_certificate::NetTrustStore>();
  while (tokenizer.GetNext()) {
    const std::string& data = tokenizer.data();
    auto* data_ptr = reinterpret_cast<const uint8_t*>(data.data());
    result->AddAnchor(
        base::span<const uint8_t>(data_ptr, data_ptr + data.size()));
  }
  return result;
}

// static
std::unique_ptr<openscreen::cast::TrustStore> CastTrustStore::Create() {
  auto cast_trust_store = std::make_unique<::cast_certificate::NetTrustStore>();
  cast_trust_store->AddAnchor(kCastRootCaDer);
  cast_trust_store->AddAnchor(kEurekaRootCaDer);
  return cast_trust_store;
}

// static
std::unique_ptr<openscreen::cast::TrustStore> CastCRLTrustStore::Create() {
  auto crl_trust_store = std::make_unique<::cast_certificate::NetTrustStore>();
  crl_trust_store->AddAnchor(kCastCRLRootCaDer);
  return crl_trust_store;
}

}  // namespace openscreen::cast

namespace cast_certificate {
namespace {

// Cast certificates rely on RSASSA-PKCS#1 v1.5 with SHA-1 for signatures.
//
// The following delegate will allow signature algorithms of:
//
//   * ECDSA, RSA-SSA, and RSA-PSS
//   * Supported EC curves: P-256, P-384, P-521.
//   * Hashes: All SHA hashes including SHA-1 (despite being known weak).
//
// It will also require RSA keys have a modulus at least 2048-bits long.
class CastPathBuilderDelegate : public net::SimplePathBuilderDelegate {
 public:
  CastPathBuilderDelegate()
      : SimplePathBuilderDelegate(
            2048,
            SimplePathBuilderDelegate::DigestPolicy::kWeakAllowSha1) {}
};

// Returns the CastCertError for the failed path building.
// This function must only be called if path building failed.
openscreen::Error::Code MapToCastError(
    const net::CertPathBuilder::Result& result) {
  DCHECK(!result.HasValidPath());
  if (result.paths.empty()) {
    return openscreen::Error::Code::kErrCertsVerifyGeneric;
  }
  if (!result.GetBestPathPossiblyInvalid()->GetTrustedCert()) {
    return openscreen::Error::Code::kErrCertsVerifyUntrustedCert;
  }
  // TODO(issuetracker.google.com/222145200): Here and elsewhere, we would like
  // to provide better error messages for logs and feedback reports.  For
  // example, collecting the certificate validity dates for a date error.
  const net::CertPathErrors& path_errors =
      result.paths.at(result.best_result_index)->errors;
  if (path_errors.ContainsError(net::cert_errors::kValidityFailedNotAfter) ||
      path_errors.ContainsError(net::cert_errors::kValidityFailedNotBefore)) {
    return openscreen::Error::Code::kErrCertsDateInvalid;
  }
  if (path_errors.ContainsError(net::cert_errors::kMaxPathLengthViolated)) {
    return openscreen::Error::Code::kErrCertsPathlen;
  }
  return openscreen::Error::Code::kErrCertsVerifyGeneric;
}

}  // namespace

NetTrustStore::NetTrustStore() = default;

NetTrustStore::~NetTrustStore() = default;

void NetTrustStore::AddAnchor(base::span<const uint8_t> data) {
  net::CertErrors errors;
  std::shared_ptr<const net::ParsedCertificate> cert =
      net::ParsedCertificate::Create(net::x509_util::CreateCryptoBuffer(data),
                                     {}, &errors);
  CHECK(cert) << errors.ToDebugString();
  // Enforce pathlen constraints and policies defined on the root certificate.
  store_.AddTrustAnchorWithConstraints(std::move(cert));
}

openscreen::ErrorOr<NetTrustStore::CertificatePathResult>
NetTrustStore::FindCertificatePath(const std::vector<std::string>& der_certs,
                                   const openscreen::cast::DateTime& time) {
  std::shared_ptr<const net::ParsedCertificate> leaf_cert;
  net::CertIssuerSourceStatic intermediate_cert_issuer_source;
  for (const std::string& der_cert : der_certs) {
    std::shared_ptr<const net::ParsedCertificate> cert(
        net::ParsedCertificate::Create(
            net::x509_util::CreateCryptoBuffer(der_cert),
            GetCertParsingOptions(), nullptr));
    if (!cert) {
      return openscreen::Error::Code::kErrCertsParse;
    }

    if (!leaf_cert) {
      leaf_cert = std::move(cert);
    } else {
      intermediate_cert_issuer_source.AddCert(std::move(cert));
    }
  }

  net::der::GeneralizedTime verification_time;
  verification_time.year = time.year;
  verification_time.month = time.month;
  verification_time.day = time.day;
  verification_time.hours = time.hour;
  verification_time.minutes = time.minute;
  verification_time.seconds = time.second;

  // Do path building and RFC 5280 compatible certificate verification using the
  // two Cast trust anchors and Cast signature policy.
  CastPathBuilderDelegate path_builder_delegate;
  net::CertPathBuilder path_builder(
      leaf_cert, &store_, &path_builder_delegate, verification_time,
      net::KeyPurpose::CLIENT_AUTH, net::InitialExplicitPolicy::kFalse,
      {net::der::Input(net::kAnyPolicyOid)},
      net::InitialPolicyMappingInhibit::kFalse,
      net::InitialAnyPolicyInhibit::kFalse);
  path_builder.AddCertIssuerSource(&intermediate_cert_issuer_source);
  net::CertPathBuilder::Result result = path_builder.Run();
  if (!result.HasValidPath()) {
    return MapToCastError(result);
  }
  const net::CertPathBuilderResultPath* path = result.GetBestValidPath();

  // Check that the leaf is valid as a _device_ certificate, which is not
  // checked by path building.
  if (!leaf_cert->has_key_usage() ||
      !leaf_cert->key_usage().AssertsBit(
          net::KEY_USAGE_BIT_DIGITAL_SIGNATURE)) {
    return openscreen::Error::Code::kErrCertsRestrictions;
  }

  CertificatePathResult out_path;
  out_path.reserve(path->certs.size());
  for (const auto& cert : path->certs) {
    out_path.push_back(std::make_unique<NetParsedCertificate>(cert));
  }

  return out_path;
}

}  // namespace cast_certificate
