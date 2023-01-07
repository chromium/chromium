// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/certificate_helper.h"

#include <cert.h>
#include <certdb.h>
#include <pk11pub.h>
#include <secport.h>

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/url_formatter/url_formatter.h"
#include "net/cert/nss_cert_database_chromeos.h"
#include "net/cert/x509_util_nss.h"

namespace ash::certificate {

namespace {

// Convert a char* return value from NSS into a std::string and free the NSS
// memory. If |nss_text| is null, |alternative_text| will be returned instead.
std::string Stringize(char* nss_text, const std::string& alternative_text) {
  if (!nss_text)
    return alternative_text;

  std::string s = nss_text;
  PORT_Free(nss_text);
  return !s.empty() ? s : alternative_text;
}

std::string GetNickname(CERTCertificate* cert_handle) {
  if (!cert_handle->nickname)
    return std::string();
  std::string name = cert_handle->nickname;
  // Hack copied from mozilla: Cut off text before first :, which seems to
  // just be the token name.
  size_t colon_pos = name.find(':');
  if (colon_pos != std::string::npos)
    name = name.substr(colon_pos + 1);
  return name;
}

}  // namespace

net::CertType GetCertType(CERTCertificate* cert_handle) {
  CERTCertTrust trust = {0};
  CERT_GetCertTrust(cert_handle, &trust);

  unsigned all_flags =
      trust.sslFlags | trust.emailFlags | trust.objectSigningFlags;

  if (cert_handle->nickname && (all_flags & CERTDB_USER))
    return net::USER_CERT;

  if ((all_flags & CERTDB_VALID_CA) || CERT_IsCACert(cert_handle, nullptr))
    return net::CA_CERT;

  // TODO(mattm): http://crbug.com/128633.
  if (trust.sslFlags & CERTDB_TERMINAL_RECORD)
    return net::SERVER_CERT;

  return net::OTHER_CERT;
}

std::string GetCertTokenName(CERTCertificate* cert_handle) {
  std::string token;
  if (cert_handle->slot)
    token = PK11_GetTokenName(cert_handle->slot);
  return token;
}

std::string GetIssuerDisplayName(CERTCertificate* cert_handle) {
  return net::x509_util::GetCERTNameDisplayName(&cert_handle->issuer);
}

std::string GetCertNameOrNickname(CERTCertificate* cert_handle) {
  std::string name = GetCertAsciiNameOrNickname(cert_handle);
  if (!name.empty())
    name = base::UTF16ToUTF8(url_formatter::IDNToUnicode(name));
  return name;
}

std::string GetCertAsciiSubjectCommonName(CERTCertificate* cert_handle) {
  return Stringize(CERT_GetCommonName(&cert_handle->subject), std::string());
}

std::string GetCertAsciiNameOrNickname(CERTCertificate* cert_handle) {
  std::string alternative_text = GetNickname(cert_handle);
  return Stringize(CERT_GetCommonName(&cert_handle->subject), alternative_text);
}

}  // namespace ash::certificate
