// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model_nss.h"

#include <cert.h>
#include <certt.h>
#include <stddef.h>

#include <algorithm>
#include <memory>
#include <tuple>

#include "chrome/third_party/mozilla_security_manager/nsNSSCertHelper.h"
#include "chrome/third_party/mozilla_security_manager/nsNSSCertificate.h"
#include "net/cert/x509_util_nss.h"

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

std::string GetRawNickname(CERTCertificate* cert_handle) {
  if (cert_handle->nickname) {
    return cert_handle->nickname;
  }
  return std::string();
}

string GetCertNameOrNickname(CERTCertificate* cert_handle) {
  string name = ProcessIDN(
      Stringize(CERT_GetCommonName(&cert_handle->subject), std::string()));
  if (!name.empty())
    return name;
  return GetNickname(cert_handle);
}

net::CertType GetType(CERTCertificate* cert_handle) {
  return psm::GetCertType(cert_handle);
}

string GetSubjectOrgName(CERTCertificate* cert_handle,
                         const string& alternative_text) {
  return Stringize(CERT_GetOrgName(&cert_handle->subject), alternative_text);
}

string GetTitle(CERTCertificate* cert_handle) {
  return psm::GetCertTitle(cert_handle);
}

std::string GetIssuerDisplayName(CERTCertificate* cert_handle) {
  return net::x509_util::GetCERTNameDisplayName(&cert_handle->issuer);
}

std::string GetSubjectDisplayName(CERTCertificate* cert_handle) {
  return net::x509_util::GetCERTNameDisplayName(&cert_handle->subject);
}

}  // namespace x509_certificate_model
