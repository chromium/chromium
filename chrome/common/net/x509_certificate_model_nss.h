// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_X509_CERTIFICATE_MODEL_NSS_H_
#define CHROME_COMMON_NET_X509_CERTIFICATE_MODEL_NSS_H_

#include <stddef.h>

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "net/cert/cert_type.h"
#include "net/cert/scoped_nss_types.h"

typedef struct CERTCertificateStr CERTCertificate;

namespace base {
class Time;
}

// This namespace defines a set of functions to be used in UI-related bits of
// X509 certificates.
namespace x509_certificate_model {

std::string GetCertNameOrNickname(CERTCertificate* cert_handle);

std::string GetVersion(CERTCertificate* cert_handle);

net::CertType GetType(CERTCertificate* cert_handle);

std::string GetSerialNumberHexified(CERTCertificate* cert_handle,
                                    const std::string& alternative_text);

std::string GetIssuerCommonName(CERTCertificate* cert_handle,
                                const std::string& alternative_text);

std::string GetIssuerOrgName(CERTCertificate* cert_handle,
                             const std::string& alternative_text);

std::string GetIssuerOrgUnitName(CERTCertificate* cert_handle,
                                 const std::string& alternative_text);

std::string GetSubjectOrgName(CERTCertificate* cert_handle,
                              const std::string& alternative_text);

std::string GetSubjectOrgUnitName(CERTCertificate* cert_handle,
                                  const std::string& alternative_text);

std::string GetSubjectCommonName(CERTCertificate* cert_handle,
                                 const std::string& alternative_text);

std::string GetIssuerDisplayName(CERTCertificate* cert_handle);
std::string GetSubjectDisplayName(CERTCertificate* cert_handle);

bool GetTimes(CERTCertificate* cert_handle,
              base::Time* issued,
              base::Time* expires);

std::string GetTitle(CERTCertificate* cert_handle);
std::string GetIssuerName(CERTCertificate* cert_handle);
std::string GetSubjectName(CERTCertificate* cert_handle);

typedef std::vector<Extension> Extensions;

void GetExtensions(const std::string& critical_label,
                   const std::string& non_critical_label,
                   CERTCertificate* cert_handle,
                   Extensions* extensions);

// Hash a certificate using the given algorithm, return the result as a
// colon-seperated hex string.
std::string HashCertSHA256(CERTCertificate* cert_handle);
std::string HashCertSHA1(CERTCertificate* cert_handle);

std::string ProcessSecAlgorithmSignature(CERTCertificate* cert_handle);
std::string ProcessSecAlgorithmSubjectPublicKey(CERTCertificate* cert_handle);
std::string ProcessSecAlgorithmSignatureWrap(CERTCertificate* cert_handle);

// Formats the public key from the X.509 SubjectPublicKeyInfo extracted from
// |cert_handle| as a string for displaying.
std::string ProcessSubjectPublicKeyInfo(CERTCertificate* cert_handle);

// Parses |public_key_spki_der| as a DER-encoded X.509 SubjectPublicKeyInfo,
// then formats the public key as a string for displaying.
std::string ProcessRawSubjectPublicKeyInfo(
    base::span<const uint8_t> public_key_spki_der);

std::string ProcessRawBitsSignatureWrap(CERTCertificate* cert_handle);

}  // namespace x509_certificate_model

#endif  // CHROME_COMMON_NET_X509_CERTIFICATE_MODEL_NSS_H_
