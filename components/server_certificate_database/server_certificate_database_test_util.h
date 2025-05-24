// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_TEST_UTIL_H_
#define COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_TEST_UTIL_H_

#include <string_view>

#include "base/test/protobuf_matchers.h"
#include "components/server_certificate_database/server_certificate_database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

MATCHER_P(CertInfoEquals, expected_value, "") {
  return expected_value.get().sha256hash_hex == arg.sha256hash_hex &&
         expected_value.get().der_cert == arg.der_cert &&
         testing::Matches(base::test::EqualsProto(
             expected_value.get().cert_metadata))(arg.cert_metadata);
}

ServerCertificateDatabase::CertInformation MakeCertInfo(
    std::string_view der_cert,
    chrome_browser_server_certificate_database::CertificateTrust::
        CertificateTrustType trust_type);

}  // namespace net

#endif  // COMPONENTS_SERVER_CERTIFICATE_DATABASE_SERVER_CERTIFICATE_DATABASE_TEST_UTIL_H_
