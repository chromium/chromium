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
      model.HashCertSHA256WithSeparators());
  EXPECT_EQ("E9 B3 96 D2 DD DF FD B3 73 BF 2C 6A D0 73 69 6A\nA2 5B 4F 68",
            model.HashCertSHA1WithSeparators());
  if (GetParam().empty()) {
    EXPECT_EQ(
        "1D7A363CE12430881EC56C9CF1409C49C491043618E598C356E2959040872F5A",
        model.GetTitle());
  } else {
    EXPECT_EQ(GetParam(), model.GetTitle());
  }
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
      model.HashCertSHA256WithSeparators());
  EXPECT_EQ("40 50 62 E5 BE FD E4 AF 97 E9 38 2A F1 6C C8 7C\n8F B7 C4 E2",
            model.HashCertSHA1WithSeparators());
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

  if (GetParam().empty())
    EXPECT_EQ("www.google.com", model.GetTitle());
  else
    EXPECT_EQ(GetParam(), model.GetTitle());

  EXPECT_EQ(
      OptionalStringOrError(
          "CN = Thawte SGC CA\nO = Thawte Consulting (Pty) Ltd.\nC = ZA\n"),
      model.GetIssuerName());
  EXPECT_EQ(OptionalStringOrError(
                "CN = www.google.com\nO = Google Inc\nL = Mountain View\nST = "
                "California\nC = US\n"),
            model.GetSubjectName());

  base::Time not_before, not_after;
  EXPECT_TRUE(model.GetTimes(&not_before, &not_after));
  // Constants copied from x509_certificate_unittest.cc.
  // Dec 18 00:00:00 2009 GMT
  const double kGoogleParseValidFrom = 1261094400;
  EXPECT_EQ(kGoogleParseValidFrom, not_before.ToDoubleT());
  // Dec 18 23:59:59 2011 GMT
  const double kGoogleParseValidTo = 1324252799;
  EXPECT_EQ(kGoogleParseValidTo, not_after.ToDoubleT());

  auto extensions = model.GetExtensions("critical", "notcrit");
  ASSERT_EQ(4U, extensions.size());
  EXPECT_EQ("Certificate Basic Constraints", extensions[0].name);
  EXPECT_EQ("critical\nIs not a Certification Authority\n",
            extensions[0].value);
  EXPECT_EQ("Extended Key Usage", extensions[2].name);
  EXPECT_EQ(
      "notcrit\nTLS WWW Server Authentication (OID.1.3.6.1.5.5.7.3.1)\nTLS WWW "
      "Client Authentication (OID.1.3.6.1.5.5.7.3.2)\nNetscape International "
      "Step-Up (OID.2.16.840.1.113730.4.1)\n",
      extensions[2].value);
  EXPECT_EQ("Authority Information Access", extensions[3].name);
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

  if (GetParam().empty())
    EXPECT_EQ("New Dream Network Certificate Authority", model.GetTitle());
  else
    EXPECT_EQ(GetParam(), model.GetTitle());

  EXPECT_EQ(OptionalStringOrError(
                "emailAddress = support@dreamhost.com\nCN = New Dream Network "
                "Certificate "
                "Authority\nOU = Security\nO = New Dream Network, LLC\nL = Los "
                "Angeles\nST = California\nC = US\n"),
            model.GetIssuerName());
  EXPECT_EQ(OptionalStringOrError(
                "emailAddress = support@dreamhost.com\nCN = New Dream Network "
                "Certificate "
                "Authority\nOU = Security\nO = New Dream Network, LLC\nL = Los "
                "Angeles\nST = California\nC = US\n"),
            model.GetSubjectName());

  base::Time not_before, not_after;
  EXPECT_TRUE(model.GetTimes(&not_before, &not_after));
  EXPECT_EQ(12800754778, not_before.ToDeltaSinceWindowsEpoch().InSeconds());
  EXPECT_EQ(13116114778, not_after.ToDeltaSinceWindowsEpoch().InSeconds());

  auto extensions = model.GetExtensions("critical", "notcrit");
  EXPECT_EQ(0U, extensions.size());
}

TEST_P(X509CertificateModel, PunyCodeCert) {
  auto cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "punycodetest.pem");
  ASSERT_TRUE(cert);
  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(cert->cert_buffer()), GetParam());
  ASSERT_TRUE(model.is_valid());
  if (GetParam().empty())
    EXPECT_EQ("xn--wgv71a119e.com", model.GetTitle());
  else
    EXPECT_EQ(GetParam(), model.GetTitle());
  EXPECT_EQ(OptionalStringOrError("xn--wgv71a119e.com"),
            model.GetIssuerCommonName());
  EXPECT_EQ(OptionalStringOrError("xn--wgv71a119e.com"),
            model.GetSubjectCommonName());
  EXPECT_EQ(OptionalStringOrError("CN = xn--wgv71a119e.com (日本語.com)\n"),
            model.GetIssuerName());
  EXPECT_EQ(OptionalStringOrError("CN = xn--wgv71a119e.com (日本語.com)\n"),
            model.GetSubjectName());
}

TEST_P(X509CertificateModel, SubjectAltNameSanityTest) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "subjectAltName_sanity_check.pem");
  ASSERT_TRUE(cert);
  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(cert->cert_buffer()), GetParam());

  auto extensions = model.GetExtensions("critical", "notcrit");
  ASSERT_EQ(2U, extensions.size());
  EXPECT_EQ("Certificate Subject Alternative Name", extensions[1].name);
  EXPECT_EQ(
      "notcrit\n"
      "OID.1.2.3.4: 0C 09 69 67 6E 6F 72 65 20 6D 65\n"
      "Email Address: test@test.example\n"
      "DNS Name: test.example\n"
      "X.500 Name: CN = 127.0.0.3\n"
      "\n"
      "IP Address: 127.0.0.2\nIP Address: fe80::1\n",
      extensions[1].value);
}

TEST_P(X509CertificateModel, GlobalsignComCert) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "2029_globalsign_com_cert.pem");
  ASSERT_TRUE(cert.get());
  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(cert->cert_buffer()), GetParam());
  ASSERT_TRUE(model.is_valid());

  auto extensions = model.GetExtensions("critical", "notcrit");
  ASSERT_EQ(9U, extensions.size());

  EXPECT_EQ("Certificate Subject Key ID", extensions[0].name);
  EXPECT_EQ(
      "notcrit\nKey ID: 59 BC D9 69 F7 B0 65 BB C8 34 C5 D2 C2 EF 17 78\nA6 "
      "47 1E 8B",
      extensions[0].value);

  EXPECT_EQ("Certification Authority Key ID", extensions[1].name);
  EXPECT_EQ(
      "notcrit\nKey ID: 8A FC 14 1B 3D A3 59 67 A5 3B E1 73 92 A6 62 91\n7F "
      "E4 78 30\n",
      extensions[1].value);

  EXPECT_EQ("Certificate Basic Constraints", extensions[4].name);
  EXPECT_EQ("notcrit\nIs not a Certification Authority\n", extensions[4].value);

  EXPECT_EQ("Certificate Key Usage", extensions[5].name);
  EXPECT_EQ(
      "critical\nSigning\nNon-repudiation\nKey Encipherment\n"
      "Data Encipherment",
      extensions[5].value);

  EXPECT_EQ("Extended Key Usage", extensions[6].name);
  EXPECT_EQ(
      "notcrit\nTLS WWW Server Authentication (OID.1.3.6.1.5.5.7.3.1)\n"
      "TLS WWW Client Authentication (OID.1.3.6.1.5.5.7.3.2)\n",
      extensions[6].value);

  EXPECT_EQ("Netscape Certificate Type", extensions[8].name);
  EXPECT_EQ("notcrit\nSSL Client Certificate\nSSL Server Certificate",
            extensions[8].value);
}

TEST_P(X509CertificateModel, DiginotarCert) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "diginotar_public_ca_2025.pem");
  ASSERT_TRUE(cert.get());
  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(cert->cert_buffer()), GetParam());
  ASSERT_TRUE(model.is_valid());

  auto extensions = model.GetExtensions("critical", "notcrit");
  ASSERT_EQ(7U, extensions.size());

  EXPECT_EQ("Certificate Basic Constraints", extensions[2].name);
  EXPECT_EQ(
      "critical\nIs a Certification Authority\n"
      "Maximum number of intermediate CAs: 0",
      extensions[2].value);
}

TEST_P(X509CertificateModel, AuthorityKeyIdentifierAllFields) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "diginotar_cyber_ca.pem");
  ASSERT_TRUE(cert.get());
  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(cert->cert_buffer()), GetParam());
  ASSERT_TRUE(model.is_valid());

  auto extensions = model.GetExtensions("critical", "notcrit");
  ASSERT_EQ(6U, extensions.size());
  EXPECT_EQ("Certification Authority Key ID", extensions[3].name);
  EXPECT_EQ(
      "notcrit\nKey ID: A6 0C 1D 9F 61 FF 07 17 B5 BF 38 46 DB 43 30 D5\n"
      "8E B0 52 06\nIssuer: X.500 Name: CN = GTE CyberTrust Global Root\n"
      "OU = GTE CyberTrust Solutions, Inc.\nO = GTE Corporation\nC = US\n\n\n"
      "Serial Number: 01 A5\n",
      extensions[3].value);
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
      bssl::UpRef(builder->GetCertBuffer()), GetParam());
  ASSERT_TRUE(model.is_valid());
  if (GetParam().empty())
    EXPECT_EQ(model.HashCertSHA256(), model.GetTitle());
  else
    EXPECT_EQ(GetParam(), model.GetTitle());
  EXPECT_EQ(OptionalStringOrError(Error()), model.GetSubjectCommonName());
  EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetSubjectOrgName());
  EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetSubjectOrgUnitName());
  EXPECT_EQ(OptionalStringOrError(Error()), model.GetSubjectName());
}

TEST_P(X509CertificateModel, SubjectInvalid) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  // SEQUENCE { SET { } }
  const uint8_t kSubject[] = {0x30, 0x02, 0x31, 0x00};
  builder->SetSubject(kSubject);

  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(builder->GetCertBuffer()), GetParam());
  EXPECT_FALSE(model.is_valid());
}

TEST_P(X509CertificateModel, SubjectEmptySequence) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  // SEQUENCE { }
  const uint8_t kSubject[] = {0x30, 0x00};
  builder->SetSubject(kSubject);

  {
    x509_certificate_model::X509CertificateModel model(
        bssl::UpRef(builder->GetCertBuffer()), GetParam());
    ASSERT_TRUE(model.is_valid());
    if (GetParam().empty())
      EXPECT_EQ(model.HashCertSHA256(), model.GetTitle());
    else
      EXPECT_EQ(GetParam(), model.GetTitle());
    EXPECT_EQ(OptionalStringOrError(NotPresent()),
              model.GetSubjectCommonName());
    EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetSubjectOrgName());
    EXPECT_EQ(OptionalStringOrError(NotPresent()),
              model.GetSubjectOrgUnitName());
    EXPECT_EQ(OptionalStringOrError(NotPresent()), model.GetSubjectName());
  }
  {
    // If subject is empty but subjectAltNames is present, GetTitle checks
    // there.
    builder->SetSubjectAltNames({"foo.com", "bar.com"}, {});
    x509_certificate_model::X509CertificateModel model(
        bssl::UpRef(builder->GetCertBuffer()), GetParam());
    ASSERT_TRUE(model.is_valid());
    if (GetParam().empty())
      EXPECT_EQ("foo.com", model.GetTitle());
    else
      EXPECT_EQ(GetParam(), model.GetTitle());
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         X509CertificateModel,
                         testing::Values(std::string(),
                                         std::string("nickname")));
