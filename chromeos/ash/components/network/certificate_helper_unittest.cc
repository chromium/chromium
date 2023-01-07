// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/certificate_helper.h"

#include "crypto/scoped_test_nss_db.h"
#include "net/cert/nss_cert_database.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

// Required to register an observer from the constructor of
// net::NSSCertDatabase.
using CertificateHelperTest = net::TestWithTaskEnvironment;

TEST_F(CertificateHelperTest, GetCertNameOrNickname) {
  net::ScopedCERTCertificate cert(net::ImportCERTCertificateFromFile(
      net::GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(cert.get());
  EXPECT_EQ("Test Root CA", certificate::GetCertNameOrNickname(cert.get()));

  net::ScopedCERTCertificate punycode_cert(net::ImportCERTCertificateFromFile(
      net::GetTestCertsDirectory(), "punycodetest.pem"));
  ASSERT_TRUE(punycode_cert.get());
  EXPECT_EQ("xn--wgv71a119e.com",
            certificate::GetCertAsciiNameOrNickname(punycode_cert.get()));
  EXPECT_EQ("日本語.com",
            certificate::GetCertNameOrNickname(punycode_cert.get()));

  net::ScopedCERTCertificate no_cn_cert(net::ImportCERTCertificateFromFile(
      net::GetTestCertsDirectory(), "no_subject_common_name_cert.pem"));
  ASSERT_TRUE(no_cn_cert.get());
  // Temp cert has no nickname.
  EXPECT_EQ("", certificate::GetCertNameOrNickname(no_cn_cert.get()));
}

TEST_F(CertificateHelperTest, GetTypeCA) {
  net::ScopedCERTCertificate cert(net::ImportCERTCertificateFromFile(
      net::GetTestCertsDirectory(), "root_ca_cert.pem"));
  ASSERT_TRUE(cert.get());

  EXPECT_EQ(net::CA_CERT, certificate::GetCertType(cert.get()));

  crypto::ScopedTestNSSDB test_nssdb;
  net::NSSCertDatabase db(crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                              test_nssdb.slot())) /* public slot */,
                          crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                              test_nssdb.slot())) /* private slot */);

  // Test that explicitly distrusted CA certs are still returned as CA_CERT
  // type. See http://crbug.com/96654.
  EXPECT_TRUE(db.SetCertTrust(cert.get(), net::CA_CERT,
                              net::NSSCertDatabase::DISTRUSTED_SSL));

  EXPECT_EQ(net::CA_CERT, certificate::GetCertType(cert.get()));
}

TEST_F(CertificateHelperTest, GetTypeServer) {
  net::ScopedCERTCertificate cert(net::ImportCERTCertificateFromFile(
      net::GetTestCertsDirectory(), "google.single.der"));
  ASSERT_TRUE(cert.get());

  // Test mozilla_security_manager::GetCertType with server certs and default
  // trust.  Currently this doesn't work.
  // TODO(mattm): make mozilla_security_manager::GetCertType smarter so we can
  // tell server certs even if they have no trust bits set.
  EXPECT_EQ(net::OTHER_CERT, certificate::GetCertType(cert.get()));

  crypto::ScopedTestNSSDB test_nssdb;
  net::NSSCertDatabase db(crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                              test_nssdb.slot())) /* public slot */,
                          crypto::ScopedPK11Slot(PK11_ReferenceSlot(
                              test_nssdb.slot())) /* private slot */);

  // Test GetCertType with server certs and explicit trust.
  EXPECT_TRUE(db.SetCertTrust(cert.get(), net::SERVER_CERT,
                              net::NSSCertDatabase::TRUSTED_SSL));

  EXPECT_EQ(net::SERVER_CERT, certificate::GetCertType(cert.get()));

  // Test GetCertType with server certs and explicit distrust.
  EXPECT_TRUE(db.SetCertTrust(cert.get(), net::SERVER_CERT,
                              net::NSSCertDatabase::DISTRUSTED_SSL));

  EXPECT_EQ(net::SERVER_CERT, certificate::GetCertType(cert.get()));
}

}  // namespace ash
