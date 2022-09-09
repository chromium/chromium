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

TEST_F(X509CertificateModelTest, GetCertNameOrNicknameAndGetTitle) {
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

  EXPECT_EQ("xn--wgv71a119e.com",
            x509_certificate_model::GetTitle(punycode_cert.get()));

  EXPECT_EQ("E=wtc@google.com",
            x509_certificate_model::GetTitle(no_cn_cert.get()));

  net::ScopedCERTCertificate no_cn_cert2(net::ImportCERTCertificateFromFile(
      net::GetTestCertsDirectory(), "ct-test-embedded-cert.pem"));
  ASSERT_TRUE(no_cn_cert2.get());
  EXPECT_EQ("L=Erw Wen,ST=Wales,O=Certificate Transparency,C=GB",
            x509_certificate_model::GetTitle(no_cn_cert2.get()));
}

TEST_F(X509CertificateModelTest, GetTypeCA) {
  net::ScopedCERTCertificate cert(net::ImportCERTCertificateFromFile(
      net::GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(cert.get());

  EXPECT_EQ(net::CA_CERT, x509_certificate_model::GetType(cert.get()));

  crypto::ScopedTestNSSDB test_nssdb;
  net::NSSCertDatabase db(crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                              test_nssdb.slot())) /* public slot */,
                          crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                              test_nssdb.slot())) /* private slot */);

  // Test that explicitly distrusted CA certs are still returned as CA_CERT
  // type. See http://crbug.com/96654.
  EXPECT_TRUE(db.SetCertTrust(cert.get(), net::CA_CERT,
                              net::NSSCertDatabase::DISTRUSTED_SSL));

  EXPECT_EQ(net::CA_CERT, x509_certificate_model::GetType(cert.get()));
}

TEST_F(X509CertificateModelTest, GetTypeServer) {
  net::ScopedCERTCertificate cert(net::ImportCERTCertificateFromFile(
      net::GetTestCertsDirectory(), "google.single.der"));
  ASSERT_TRUE(cert.get());

  // Test mozilla_security_manager::GetCertType with server certs and default
  // trust.  Currently this doesn't work.
  // TODO(mattm): make mozilla_security_manager::GetCertType smarter so we can
  // tell server certs even if they have no trust bits set.
  EXPECT_EQ(net::OTHER_CERT, x509_certificate_model::GetType(cert.get()));

  crypto::ScopedTestNSSDB test_nssdb;
  net::NSSCertDatabase db(crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                              test_nssdb.slot())) /* public slot */,
                          crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                              test_nssdb.slot())) /* private slot */);

  // Test GetCertType with server certs and explicit trust.
  EXPECT_TRUE(db.SetCertTrust(cert.get(), net::SERVER_CERT,
                              net::NSSCertDatabase::TRUSTED_SSL));

  EXPECT_EQ(net::SERVER_CERT, x509_certificate_model::GetType(cert.get()));

  // Test GetCertType with server certs and explicit distrust.
  EXPECT_TRUE(db.SetCertTrust(cert.get(), net::SERVER_CERT,
                              net::NSSCertDatabase::DISTRUSTED_SSL));

  EXPECT_EQ(net::SERVER_CERT, x509_certificate_model::GetType(cert.get()));
}
