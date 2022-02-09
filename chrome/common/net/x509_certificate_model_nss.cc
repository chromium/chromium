// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model_nss.h"

#include <cert.h>
#include <certt.h>
#include <hasht.h>
#include <keyhi.h>  // SECKEY_DestroyPrivateKey
#include <keythi.h>  // SECKEYPrivateKey
#include <pk11pub.h>  // PK11_FindKeyByAnyCert
#include <seccomon.h>  // SECItem
#include <sechash.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unicode/uidna.h>

#include <algorithm>
#include <memory>
#include <tuple>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/third_party/mozilla_security_manager/nsNSSCertHelper.h"
#include "chrome/third_party/mozilla_security_manager/nsNSSCertificate.h"
#include "crypto/nss_key_util.h"
#include "crypto/nss_util.h"
#include "crypto/scoped_nss_types.h"
#include "net/cert/x509_util_nss.h"
#include "ui/base/l10n/l10n_util.h"

namespace psm = mozilla_security_manager;

namespace {

// Convert a char* return value from NSS into a std::string and free the NSS
// memory.  If the arg is NULL, an empty string will be returned instead.
std::string Stringize(char* nss_text, const std::string& alternative_text) {
  if (!nss_text)
    return alternative_text;

  std::string s = nss_text;
  PORT_Free(nss_text);
  return s;
}

// Hash a certificate using the given algorithm, return the result as a
// colon-seperated hex string.  The len specified is the number of bytes
// required for storing the raw fingerprint.
// (It's a bit redundant that the caller needs to specify len in addition to the
// algorithm, but given the limited uses, not worth fixing.)
std::string HashCert(CERTCertificate* cert, HASH_HashType algorithm, int len) {
  unsigned char fingerprint[HASH_LENGTH_MAX];

  DCHECK(NULL != cert->derCert.data);
  DCHECK_NE(0U, cert->derCert.len);
  DCHECK_LE(len, HASH_LENGTH_MAX);
  memset(fingerprint, 0, len);
  SECStatus rv = HASH_HashBuf(algorithm, fingerprint, cert->derCert.data,
                              cert->derCert.len);
  DCHECK_EQ(rv, SECSuccess);
  return x509_certificate_model::ProcessRawBytes(fingerprint, len);
}

std::string ProcessSecAlgorithmInternal(SECAlgorithmID* algorithm_id) {
  return psm::GetOIDText(&algorithm_id->algorithm);
}

std::string ProcessExtension(
    const std::string& critical_label,
    const std::string& non_critical_label,
    CERTCertExtension* extension) {
  std::string criticality =
      extension->critical.data && extension->critical.data[0] ?
          critical_label : non_critical_label;
  return criticality + "\n" + psm::ProcessExtensionData(extension);
}

std::string GetNickname(CERTCertificate* cert_handle) {
  std::string name;
  if (cert_handle->nickname) {
    name = cert_handle->nickname;
    // Hack copied from mozilla: Cut off text before first :, which seems to
    // just be the token name.
    size_t colon_pos = name.find(':');
    if (colon_pos != std::string::npos)
      name = name.substr(colon_pos + 1);
  }
  return name;
}

}  // namespace

namespace x509_certificate_model {

using std::string;

string GetCertNameOrNickname(CERTCertificate* cert_handle) {
  string name = ProcessIDN(
      Stringize(CERT_GetCommonName(&cert_handle->subject), std::string()));
  if (!name.empty())
    return name;
  return GetNickname(cert_handle);
}

string GetVersion(CERTCertificate* cert_handle) {
  // If the version field is omitted from the certificate, the default
  // value is v1(0).
  unsigned long version = 0;
  if (cert_handle->version.len == 0 ||
      SEC_ASN1DecodeInteger(&cert_handle->version, &version) == SECSuccess) {
    return base::NumberToString(base::strict_cast<uint64_t>(version + 1));
  }
  return std::string();
}

net::CertType GetType(CERTCertificate* cert_handle) {
  return psm::GetCertType(cert_handle);
}

string GetSerialNumberHexified(CERTCertificate* cert_handle,
                               const string& alternative_text) {
  return Stringize(CERT_Hexify(&cert_handle->serialNumber, true),
                   alternative_text);
}

string GetIssuerCommonName(CERTCertificate* cert_handle,
                           const string& alternative_text) {
  return Stringize(CERT_GetCommonName(&cert_handle->issuer), alternative_text);
}

string GetIssuerOrgName(CERTCertificate* cert_handle,
                        const string& alternative_text) {
  return Stringize(CERT_GetOrgName(&cert_handle->issuer), alternative_text);
}

string GetIssuerOrgUnitName(CERTCertificate* cert_handle,
                            const string& alternative_text) {
  return Stringize(CERT_GetOrgUnitName(&cert_handle->issuer), alternative_text);
}

string GetSubjectOrgName(CERTCertificate* cert_handle,
                         const string& alternative_text) {
  return Stringize(CERT_GetOrgName(&cert_handle->subject), alternative_text);
}

string GetSubjectOrgUnitName(CERTCertificate* cert_handle,
                             const string& alternative_text) {
  return Stringize(CERT_GetOrgUnitName(&cert_handle->subject),
                   alternative_text);
}

string GetSubjectCommonName(CERTCertificate* cert_handle,
                            const string& alternative_text) {
  return Stringize(CERT_GetCommonName(&cert_handle->subject), alternative_text);
}

bool GetTimes(CERTCertificate* cert_handle,
              base::Time* issued,
              base::Time* expires) {
  return net::x509_util::GetValidityTimes(cert_handle, issued, expires);
}

string GetTitle(CERTCertificate* cert_handle) {
  return psm::GetCertTitle(cert_handle);
}

string GetIssuerName(CERTCertificate* cert_handle) {
  return psm::ProcessName(&cert_handle->issuer);
}

string GetSubjectName(CERTCertificate* cert_handle) {
  return psm::ProcessName(&cert_handle->subject);
}

std::string GetIssuerDisplayName(CERTCertificate* cert_handle) {
  return net::x509_util::GetCERTNameDisplayName(&cert_handle->issuer);
}

std::string GetSubjectDisplayName(CERTCertificate* cert_handle) {
  return net::x509_util::GetCERTNameDisplayName(&cert_handle->subject);
}

void GetExtensions(const string& critical_label,
                   const string& non_critical_label,
                   CERTCertificate* cert_handle,
                   Extensions* extensions) {
  if (cert_handle->extensions) {
    for (size_t i = 0; cert_handle->extensions[i] != NULL; ++i) {
      Extension extension;
      extension.name = psm::GetOIDText(&cert_handle->extensions[i]->id);
      extension.value = ProcessExtension(
          critical_label, non_critical_label, cert_handle->extensions[i]);
      extensions->push_back(extension);
    }
  }
}

string HashCertSHA256(CERTCertificate* cert_handle) {
  return HashCert(cert_handle, HASH_AlgSHA256, SHA256_LENGTH);
}

string HashCertSHA1(CERTCertificate* cert_handle) {
  return HashCert(cert_handle, HASH_AlgSHA1, SHA1_LENGTH);
}

string ProcessSecAlgorithmSignature(CERTCertificate* cert_handle) {
  return ProcessSecAlgorithmInternal(&cert_handle->signature);
}

string ProcessSecAlgorithmSubjectPublicKey(CERTCertificate* cert_handle) {
  return ProcessSecAlgorithmInternal(
      &cert_handle->subjectPublicKeyInfo.algorithm);
}

string ProcessSecAlgorithmSignatureWrap(CERTCertificate* cert_handle) {
  return ProcessSecAlgorithmInternal(
      &cert_handle->signatureWrap.signatureAlgorithm);
}

string ProcessSubjectPublicKeyInfo(CERTCertificate* cert_handle) {
  return psm::ProcessSubjectPublicKeyInfo(&cert_handle->subjectPublicKeyInfo);
}

string ProcessRawSubjectPublicKeyInfo(base::span<const uint8_t> spki_der) {
  crypto::ScopedCERTSubjectPublicKeyInfo spki =
      crypto::DecodeSubjectPublicKeyInfoNSS(spki_der);
  if (!spki)
    return std::string();
  return psm::ProcessSubjectPublicKeyInfo(spki.get());
}

string ProcessRawBitsSignatureWrap(CERTCertificate* cert_handle) {
  return ProcessRawBits(cert_handle->signatureWrap.signature.data,
                        cert_handle->signatureWrap.signature.len);
}

}  // namespace x509_certificate_model
