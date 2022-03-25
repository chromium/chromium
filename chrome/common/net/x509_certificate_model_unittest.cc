// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/x509_certificate_model.h"

#include "net/cert/x509_util.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

class X509CertificateModel : public testing::TestWithParam<std::string> {};

using x509_certificate_model::Error;
using x509_certificate_model::NotPresent;
using x509_certificate_model::OptionalStringOrError;

TEST_P(X509CertificateModel, InvalidCert) {
  x509_certificate_model::X509CertificateModel model(
      net::x509_util::CreateCryptoBuffer(
          base::span<const uint8_t>({'b', 'a', 'd', '\n'})),
      GetParam());

  EXPECT_EQ(
      "1D 7A 36 3C E1 24 30 88 1E C5 6C 9C F1 40 9C 49\nC4 91 04 36 18 E5 98 "
      "C3 56 E2 95 90 40 87 2F 5A",
      model.HashCertSHA256());
  EXPECT_EQ("E9 B3 96 D2 DD DF FD B3 73 BF 2C 6A D0 73 69 6A\nA2 5B 4F 68",
            model.HashCertSHA1());
  EXPECT_FALSE(model.is_valid());
}

TEST_P(X509CertificateModel, GetGoogleCertFields) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "google.single.pem");
  ASSERT_TRUE(cert);
  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(cert->cert_buffer()), GetParam());
  EXPECT_EQ(
      "F6 41 C3 6C FE F4 9B C0 71 35 9E CF 88 EE D9 31\n7B 73 8B 59 89 41 6A "
      "D4 01 72 0C 0A 4E 2E 63 52",
      model.HashCertSHA256());
  EXPECT_EQ("40 50 62 E5 BE FD E4 AF 97 E9 38 2A F1 6C C8 7C\n8F B7 C4 E2",
            model.HashCertSHA1());
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ("3", model.GetVersion());
  EXPECT_EQ("2F:DF:BC:F6:AE:91:52:6D:0F:9A:A3:DF:40:34:3E:9A",
            model.GetSerialNumberHexified());
  EXPECT_EQ(OptionalStringOrError("Thawte SGC CA"),
            model.GetIssuerCommonName());
  EXPECT_EQ(OptionalStringOrError("Thawte Consulting (Pty) Ltd."),
            model.GetIssuerOrgName());
  EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetIssuerOrgUnitName());

  EXPECT_EQ(OptionalStringOrError("www.google.com"),
            model.GetSubjectCommonName());
  EXPECT_EQ(OptionalStringOrError("Google Inc"), model.GetSubjectOrgName());
  EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetSubjectOrgUnitName());
}

TEST_P(X509CertificateModel, GetNDNCertFields) {
  auto cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ndn.ca.crt");
  ASSERT_TRUE(cert);
  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(cert->cert_buffer()), GetParam());
  ASSERT_TRUE(model.is_valid());
  EXPECT_EQ("1", model.GetVersion());
  // The model just returns the hex of the DER bytes, so the leading zeros are
  // included.
  EXPECT_EQ("00:DB:B7:C6:06:47:AF:37:A2", model.GetSerialNumberHexified());

  EXPECT_EQ(OptionalStringOrError("New Dream Network Certificate Authority"),
            model.GetIssuerCommonName());
  EXPECT_EQ(OptionalStringOrError("New Dream Network, LLC"),
            model.GetIssuerOrgName());
  EXPECT_EQ(OptionalStringOrError("Security"), model.GetIssuerOrgUnitName());
  EXPECT_EQ(OptionalStringOrError("New Dream Network Certificate Authority"),
            model.GetSubjectCommonName());
  EXPECT_EQ(OptionalStringOrError("New Dream Network, LLC"),
            model.GetSubjectOrgName());
  EXPECT_EQ(OptionalStringOrError("Security"), model.GetSubjectOrgUnitName());
}

TEST_P(X509CertificateModel, SubjectIA5StringInvalidCharacters) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  // SEQUENCE {
  //   SET {
  //     SEQUENCE {
  //       # commonName
  //       OBJECT_IDENTIFIER { 2.5.4.3 }
  //       # Not a valid IA5String:
  //       IA5String { "a \xf6 b" }
  //     }
  //   }
  // }
  const uint8_t kSubject[] = {0x30, 0x10, 0x31, 0x0e, 0x30, 0x0c,
                              0x06, 0x03, 0x55, 0x04, 0x03, 0x16,
                              0x05, 0x61, 0x20, 0xf6, 0x20, 0x62};
  builder->SetSubject(kSubject);

  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(builder->GetCertBuffer()), "");
  ASSERT_TRUE(model.is_valid());
  EXPECT_EQ(OptionalStringOrError(Error()), model.GetSubjectCommonName());
  EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetSubjectOrgName());
  EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetSubjectOrgUnitName());
}

INSTANTIATE_TEST_SUITE_P(All,
                         X509CertificateModel,
                         testing::Values(std::string(),
                                         std::string("nickname")));
