// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/server_certificate_database/server_certificate_database_test_util.h"

#include "base/containers/span.h"

namespace net {

ServerCertificateDatabase::CertInformation MakeCertInfo(
    std::string_view der_cert,
    chrome_browser_server_certificate_database::CertificateTrust::
        CertificateTrustType trust_type) {
  ServerCertificateDatabase::CertInformation cert_info(
      base::as_byte_span(der_cert));
  cert_info.cert_metadata.mutable_trust()->set_trust_type(trust_type);
  return cert_info;
}

}  // namespace net
