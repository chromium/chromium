// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model_nss.h"

#include <stddef.h>

#include "base/files/file_path.h"
#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database.h"
#include "net/cert/scoped_nss_types.h"
#include "net/cert/x509_util_nss.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

// Required to register an observer from the constructor of
// net::NSSCertDatabase.
using X509CertificateModelTest = net::TestWithTaskEnvironment;

TEST_F(X509CertificateModelTest, GetCertNameOrNickname) {
  net::ScopedCERTCertificate cert(net::ImportCERTCertificateFromFile(
      net::GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(cert.get());
  EXPECT_EQ("Test Root CA",
            x509_certificate_model::GetCertNameOrNickname(cert.get()));

  net::ScopedCERTCertificate punycode_cert(net::ImportCERTCertificateFromFile(
      net::GetTestCertsDirectory(), "punycodetest.pem"));
  ASSERT_TRUE(punycode_cert.get());
  EXPECT_EQ("xn--wgv71a119e.com (日本語.com)",
            x509_certificate_model::GetCertNameOrNickname(punycode_cert.get()));

  net::ScopedCERTCertificate no_cn_cert(net::ImportCERTCertificateFromFile(
      net::GetTestCertsDirectory(), "no_subject_common_name_cert.pem"));
  ASSERT_TRUE(no_cn_cert.get());
  // Temp cert has no nickname.
  EXPECT_EQ("",
            x509_certificate_model::GetCertNameOrNickname(no_cn_cert.get()));
}
