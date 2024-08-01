// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/common/net/x509_certificate_model.h"

#include <string_view>

#include "net/cert/x509_util.h"
#include "net/test/cert_builder.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/parse_certificate.h"

class X509CertificateModel : public testing::TestWithParam<std::string> {};

using x509_certificate_model::Error;
using x509_certificate_model::NotPresent;
using x509_certificate_model::OptionalStringOrError;

namespace {

std::optional<std::string> FindExtension(
    const std::vector<x509_certificate_model::Extension>& extensions,
    std::string_view name) {
  for (const auto& extension : extensions) {
    if (extension.name == name) {
      return extension.value;
    }
  }

  return std::nullopt;
}

}  // namespace

TEST_P(X509CertificateModel, InvalidCert) {
  x509_certificate_model::X509CertificateModel model(
      net::x509_util::CreateCryptoBuffer(
          base::span<const uint8_t>({'b', 'a', 'd', '\n'})),
      GetParam());

  EXPECT_EQ("1d7a363ce12430881ec56c9cf1409c49c491043618e598c356e2959040872f5a",
            model.HashCertSHA256());
  if (GetParam().empty()) {
    EXPECT_EQ(
        "1d7a363ce12430881ec56c9cf1409c49c491043618e598c356e2959040872f5a",
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
  EXPECT_EQ("f641c36cfef49bc071359ecf88eed9317b738b5989416ad401720c0a4e2e6352",
            model.HashCertSHA256());
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ("23a55ce68ea1b20623de09e93fdf3bb03287ac737b27335b4307fe9ec4855c34",
            model.HashSpkiSHA256());
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
  EXPECT_EQ(kGoogleParseValidFrom, not_before.InSecondsFSinceUnixEpoch());
  // Dec 18 23:59:59 2011 GMT
  const double kGoogleParseValidTo = 1324252799;
  EXPECT_EQ(kGoogleParseValidTo, not_after.InSecondsFSinceUnixEpoch());

  EXPECT_EQ("PKCS #1 SHA-1 With RSA Encryption",
            model.ProcessSecAlgorithmSignature());
  EXPECT_EQ("PKCS #1 SHA-1 With RSA Encryption",
            model.ProcessSecAlgorithmSignatureWrap());
  EXPECT_EQ("PKCS #1 RSA Encryption",
            model.ProcessSecAlgorithmSubjectPublicKey());
  EXPECT_EQ(
      "Modulus (1024 bits):\n"
      "  E8 F9 86 0F 90 FA 86 D7 DF BD 72 26 B6 D7 44 02\n"
      "83 78 73 D9 02 28 EF 88 45 39 FB 10 E8 7C AE A9\n"
      "38 D5 75 C6 38 EB 0A 15 07 9B 83 E8 CD 82 D5 E3\n"
      "F7 15 68 45 A1 0B 19 85 BC E2 EF 84 E7 DD F2 D7\n"
      "B8 98 C2 A1 BB B5 C1 51 DF D4 83 02 A7 3D 06 42\n"
      "5B E1 22 C3 DE 6B 85 5F 1C D6 DA 4E 8B D3 9B EE\n"
      "B9 67 22 2A 1D 11 EF 79 A4 B3 37 8A F4 FE 18 FD\n"
      "BC F9 46 23 50 97 F3 AC FC 24 46 2B 5C 3B B7 45\n"
      "\n"
      "  Public Exponent (17 bits):\n"
      "  01 00 01",
      model.ProcessSubjectPublicKeyInfo());
  EXPECT_EQ(
      "9F 43 CF 5B C4 50 29 B1 BF E2 B0 9A FF 6A 21 1D\n"
      "2D 12 C3 2C 4E 5A F9 12 E2 CE B9 82 52 2D E7 1D\n"
      "7E 1A 76 96 90 79 D1 24 52 38 79 BB 63 8D 80 97\n"
      "7C 23 20 0F 91 4D 16 B9 EA EE F4 6D 89 CA C6 BD\n"
      "CC 24 68 D6 43 5B CE 2A 58 BF 3C 18 E0 E0 3C 62\n"
      "CF 96 02 2D 28 47 50 34 E1 27 BA CF 99 D1 50 FF\n"
      "29 25 C0 36 36 15 33 52 70 BE 31 8F 9F E8 7F E7\n"
      "11 0C 8D BF 84 A0 42 1A 80 89 B0 31 58 41 07 5F",
      model.ProcessRawBitsSignatureWrap());

  auto extensions = model.GetExtensions("critical", "notcrit");
  ASSERT_EQ(4U, extensions.size());
  EXPECT_EQ("Certificate Basic Constraints", extensions[0].name);
  EXPECT_EQ("critical\nIs not a Certification Authority\n",
            extensions[0].value);
  EXPECT_EQ("CRL Distribution Points", extensions[1].name);
  EXPECT_EQ("notcrit\nURI: http://crl.thawte.com/ThawteSGCCA.crl\n",
            extensions[1].value);
  EXPECT_EQ("Extended Key Usage", extensions[2].name);
  EXPECT_EQ(
      "notcrit\nTLS WWW Server Authentication (OID.1.3.6.1.5.5.7.3.1)\nTLS WWW "
      "Client Authentication (OID.1.3.6.1.5.5.7.3.2)\nNetscape International "
      "Step-Up (OID.2.16.840.1.113730.4.1)\n",
      extensions[2].value);
  EXPECT_EQ("Authority Information Access", extensions[3].name);
  EXPECT_EQ(
      "notcrit\nOCSP Responder: URI: http://ocsp.thawte.com\nCA Issuers: URI: "
      "http://www.thawte.com/repository/Thawte_SGC_CA.crt\n",
      extensions[3].value);
}

TEST_P(X509CertificateModel, GetSCTField) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "lets-encrypt-dst-x3-root.pem");
  ASSERT_TRUE(cert);
  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(cert->cert_buffer()), GetParam());
  ASSERT_TRUE(model.is_valid());

  EXPECT_EQ("3", model.GetVersion());
  EXPECT_EQ("04:7B:F4:FD:2C:FB:01:92:D5:30:C1:0F:C9:19:83:2A:49:EF",
            model.GetSerialNumberHexified());

  auto extensions = model.GetExtensions("critical", "notcrit");
  auto extension_value =
      FindExtension(extensions, "Signed Certificate Timestamp List");
  ASSERT_TRUE(extension_value);
  EXPECT_EQ(
      "notcrit\n"
      "04 81 F1 00 EF 00 76 00 41 C8 CA B1 DF 22 46 4A\n"
      "10 C6 A1 3A 09 42 87 5E 4E 31 8B 1B 03 EB EB 4B\n"
      "C7 68 F0 90 62 96 06 F6 00 00 01 7E 17 63 85 3D\n"
      "00 00 04 03 00 47 30 45 02 20 05 FB 47 45 BD 63\n"
      "AD FD E7 AF 9E 7E D6 51 5A 1E AB 62 FE 2A 27 4B\n"
      "A0 ED 8A 4A 8F B3 C8 36 8C BD 02 21 00 8B 07 10\n"
      "4C BF 07 1C ED 54 DF 28 2C E3 B2 32 6B 43 48 E4\n"
      "04 80 28 17 91 50 8D 28 FC 58 08 BF 7C 00 75 00\n"
      "46 A5 55 EB 75 FA 91 20 30 B5 A2 89 69 F4 F3 7D\n"
      "11 2C 41 74 BE FD 49 B8 85 AB F2 FC 70 FE 6D 47\n"
      "00 00 01 7E 17 63 85 53 00 00 04 03 00 46 30 44\n"
      "02 20 73 8C D6 ED CC 59 2D 3D 5E 1A 37 E9 42 A2\n"
      "74 6D 95 1B 20 0E 19 91 40 0E AD A3 80 66 48 FB\n"
      "17 32 02 20 02 3A 61 DA 61 EF CB 37 BB 97 5E AC\n"
      "79 08 2B 5E 71 EA 9B 7B FC B4 F5 50 04 2E E0 40\n"
      "42 44 2C 79",
      extension_value);
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
  ASSERT_EQ(3U, extensions.size());
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

TEST_P(X509CertificateModel, CertificatePoliciesSanityTest) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "policies_sanity_check.pem");
  ASSERT_TRUE(cert);
  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(cert->cert_buffer()), GetParam());

  auto extensions = model.GetExtensions("critical", "notcrit");
  ASSERT_EQ(2U, extensions.size());
  EXPECT_EQ("Certificate Policies", extensions[0].name);
  EXPECT_EQ(
      "notcrit\nOID.1.2.3.4.5\nOID.1.3.5.8.12:\n"
      "  Certification Practice Statement Pointer:"
      "    http://cps.example.com/foo\n"
      "  User Notice:Organization Name - #1, #2, #3, #4\n"
      "    Explicit Text Here\n"
      "  User Notice:\n    Explicit Text Two\n"
      "  User Notice:Organization Name Two - #42\n",
      extensions[0].value);
}

TEST_P(X509CertificateModel, CertificatePoliciesInvalidUtf8UserNotice) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  // \xa1 is a UTF-8 continuation byte, but there is no leading byte before it,
  // which is invalid.
  //
  // SEQUENCE {
  //   SEQUENCE {
  //     OBJECT_IDENTIFIER { 1.2.3 }
  //     SEQUENCE {
  //       SEQUENCE {
  //         # unotice
  //         OBJECT_IDENTIFIER { 1.3.6.1.5.5.7.2.2 }
  //         SEQUENCE {
  //           # explicitText
  //           UTF8String { "Explicit \xa1 Text" }
  //         }
  //       }
  //     }
  //   }
  // }
  const uint8_t kExtension[] = {
      0x30, 0x27, 0x30, 0x25, 0x06, 0x02, 0x2a, 0x03, 0x30, 0x1f, 0x30,
      0x1d, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x02, 0x02,
      0x30, 0x11, 0x0c, 0x0f, 0x45, 0x78, 0x70, 0x6c, 0x69, 0x63, 0x69,
      0x74, 0x20, 0xa1, 0x20, 0x54, 0x65, 0x78, 0x74};
  builder->SetExtension(
      bssl::der::Input(bssl::kCertificatePoliciesOid),
      std::string(kExtension, kExtension + sizeof(kExtension)));

  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(builder->GetCertBuffer()), GetParam());
  ASSERT_TRUE(model.is_valid());
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "policies_sanity_check.pem");
  ASSERT_TRUE(cert);
  auto extensions = model.GetExtensions("critical", "notcrit");
  auto extension_value = FindExtension(extensions, "Certificate Policies");
  ASSERT_TRUE(extension_value);
  EXPECT_EQ("notcrit\nError: Unable to decode extension", *extension_value);
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

  EXPECT_EQ("Authority Information Access", extensions[2].name);
  EXPECT_EQ(
      "notcrit\nCA Issuers: "
      "URI: http://secure.globalsign.net/cacert/SHA256extendval1.crt\n",
      extensions[2].value);

  EXPECT_EQ("CRL Distribution Points", extensions[3].name);
  EXPECT_EQ("notcrit\nURI: http://crl.globalsign.net/SHA256ExtendVal1.crl\n",
            extensions[3].value);

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

  EXPECT_EQ("Certificate Policies", extensions[7].name);
  EXPECT_EQ(
      "notcrit\nOID.1.3.6.1.4.1.4146.1.1:\n"
      "  Certification Practice Statement Pointer:"
      "    http://www.globalsign.net/repository/\n",
      extensions[7].value);

  EXPECT_EQ("Netscape Certificate Type", extensions[8].name);
  EXPECT_EQ("notcrit\nSSL Client Certificate\nSSL Server Certificate",
            extensions[8].value);
}

TEST_P(X509CertificateModel, NSCertComment) {
  auto cert = net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "foaf.me.chromium-test-cert.der");
  ASSERT_TRUE(cert.get());
  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(cert->cert_buffer()), GetParam());
  ASSERT_TRUE(model.is_valid());

  auto extensions = model.GetExtensions("critical", "notcrit");
  ASSERT_EQ(5U, extensions.size());
  EXPECT_EQ("Netscape Certificate Comment", extensions[1].name);
  EXPECT_EQ("notcrit\nOpenSSL Generated Certificate", extensions[1].value);
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

  EXPECT_EQ("Authority Information Access", extensions[0].name);
  EXPECT_EQ(
      "notcrit\nOCSP Responder: "
      "URI: http://validation.diginotar.nl\n",
      extensions[0].value);

  EXPECT_EQ("Certificate Basic Constraints", extensions[2].name);
  EXPECT_EQ(
      "critical\nIs a Certification Authority\n"
      "Maximum number of intermediate CAs: 0",
      extensions[2].value);

  EXPECT_EQ("Certificate Policies", extensions[3].name);
  EXPECT_EQ(
      "notcrit\nOID.2.16.528.1.1001.1.1.1.1.5.2.6.4:\n"
      "  Certification Practice Statement Pointer:"
      "    http://www.diginotar.nl/cps\n"
      "  User Notice:\n"
      "    Conditions, as mentioned on our website (www.diginotar.nl), are "
      "applicable to all our products and services.\n",
      extensions[3].value);
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

TEST_P(X509CertificateModel, CrlDpCrlIssuerAndRelativeName) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  // SEQUENCE {
  //   SEQUENCE {
  //     [0] {
  //       [1] {
  //         SEQUENCE {
  //           # commonName
  //           OBJECT_IDENTIFIER { 2.5.4.3 }
  //           PrintableString { "indirect CRL for indirectCRL CA3" }
  //         }
  //       }
  //     }
  //     [2] {
  //       [4] {
  //         SEQUENCE {
  //           SET {
  //             SEQUENCE {
  //               # organizationUnitName
  //               OBJECT_IDENTIFIER { 2.5.4.11 }
  //               PrintableString { "indirectCRL CA3 cRLIssuer" }
  //             }
  //           }
  //         }
  //       }
  //     }
  //   }
  // }
  const uint8_t kCrldp[] = {
      0x30, 0x59, 0x30, 0x57, 0xa0, 0x2b, 0xa1, 0x29, 0x30, 0x27, 0x06, 0x03,
      0x55, 0x04, 0x03, 0x13, 0x20, 0x69, 0x6e, 0x64, 0x69, 0x72, 0x65, 0x63,
      0x74, 0x20, 0x43, 0x52, 0x4c, 0x20, 0x66, 0x6f, 0x72, 0x20, 0x69, 0x6e,
      0x64, 0x69, 0x72, 0x65, 0x63, 0x74, 0x43, 0x52, 0x4c, 0x20, 0x43, 0x41,
      0x33, 0xa2, 0x28, 0xa4, 0x26, 0x30, 0x24, 0x31, 0x22, 0x30, 0x20, 0x06,
      0x03, 0x55, 0x04, 0x0b, 0x13, 0x19, 0x69, 0x6e, 0x64, 0x69, 0x72, 0x65,
      0x63, 0x74, 0x43, 0x52, 0x4c, 0x20, 0x43, 0x41, 0x33, 0x20, 0x63, 0x52,
      0x4c, 0x49, 0x73, 0x73, 0x75, 0x65, 0x72};

  builder->SetExtension(bssl::der::Input(bssl::kCrlDistributionPointsOid),
                        std::string(kCrldp, kCrldp + sizeof(kCrldp)));

  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(builder->GetCertBuffer()), GetParam());
  ASSERT_TRUE(model.is_valid());

  auto extensions = model.GetExtensions("critical", "notcrit");

  auto extension_value = FindExtension(extensions, "CRL Distribution Points");
  ASSERT_TRUE(extension_value);
  EXPECT_EQ(
      "notcrit\nCN = indirect CRL for indirectCRL CA3\nIssuer: X.500 Name: OU "
      "= indirectCRL CA3 cRLIssuer\n\n",
      *extension_value);
}

TEST_P(X509CertificateModel, CrlDpReasons) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  // SEQUENCE {
  //   SEQUENCE {
  //     [0] {
  //       [0] {
  //         [4] {
  //           SEQUENCE {
  //             SET {
  //               SEQUENCE {
  //                 # commonName
  //                 OBJECT_IDENTIFIER { 2.5.4.3 }
  //                 PrintableString { "CRL1" }
  //               }
  //             }
  //           }
  //         }
  //       }
  //     }
  //     [1 PRIMITIVE] { `0560` }
  //   }
  //   SEQUENCE {
  //     [0] {
  //       [0] {
  //         [4] {
  //           SEQUENCE {
  //             SET {
  //               SEQUENCE {
  //                 # commonName
  //                 OBJECT_IDENTIFIER { 2.5.4.3 }
  //                 PrintableString { "CRL2" }
  //               }
  //             }
  //           }
  //         }
  //       }
  //     }
  //     [1 PRIMITIVE] { `079f80` }
  //   }
  // }
  const uint8_t kCrldp[] = {
      0x30, 0x3b, 0x30, 0x1b, 0xa0, 0x15, 0xa0, 0x13, 0xa4, 0x11, 0x30,
      0x0f, 0x31, 0x0d, 0x30, 0x0b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13,
      0x04, 0x43, 0x52, 0x4c, 0x31, 0x81, 0x02, 0x05, 0x60, 0x30, 0x1c,
      0xa0, 0x15, 0xa0, 0x13, 0xa4, 0x11, 0x30, 0x0f, 0x31, 0x0d, 0x30,
      0x0b, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x04, 0x43, 0x52, 0x4c,
      0x32, 0x81, 0x03, 0x07, 0x9f, 0x80};

  builder->SetExtension(bssl::der::Input(bssl::kCrlDistributionPointsOid),
                        std::string(kCrldp, kCrldp + sizeof(kCrldp)));

  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(builder->GetCertBuffer()), GetParam());
  ASSERT_TRUE(model.is_valid());

  auto extensions = model.GetExtensions("critical", "notcrit");

  auto extension_value = FindExtension(extensions, "CRL Distribution Points");
  ASSERT_TRUE(extension_value);

  EXPECT_EQ(
      "notcrit\nX.500 Name: CN = CRL1\n\nKey Compromise,CA Compromise\nX.500 "
      "Name: CN = CRL2\n\nUnused,Affiliation Changed,Superseded,Cessation of "
      "Operation,Certificate on Hold\n",
      *extension_value);
}

TEST_P(X509CertificateModel, AuthorityInfoAccessNonstandardOidAndLocationType) {
  base::FilePath certs_dir = net::GetTestCertsDirectory();
  std::unique_ptr<net::CertBuilder> builder =
      net::CertBuilder::FromFile(certs_dir.AppendASCII("ok_cert.pem"), nullptr);
  ASSERT_TRUE(builder);

  // SEQUENCE {
  //  SEQUENCE {
  //    OBJECT_IDENTIFIER { 1.4.9.20 }
  //    [1 PRIMITIVE] { "foo@example.com" }
  //  }
  // }
  const uint8_t kAIA[] = {0x30, 0x18, 0x30, 0x16, 0x06, 0x03, 0x2c, 0x09, 0x14,
                          0x81, 0x0f, 0x66, 0x6f, 0x6f, 0x40, 0x65, 0x78, 0x61,
                          0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d};
  builder->SetExtension(bssl::der::Input(bssl::kAuthorityInfoAccessOid),
                        std::string(kAIA, kAIA + sizeof(kAIA)));

  x509_certificate_model::X509CertificateModel model(
      bssl::UpRef(builder->GetCertBuffer()), GetParam());
  ASSERT_TRUE(model.is_valid());

  auto extensions = model.GetExtensions("critical", "notcrit");

  auto extension_value =
      FindExtension(extensions, "Authority Information Access");
  ASSERT_TRUE(extension_value);
  EXPECT_EQ("notcrit\nOID.1.4.9.20: Email Address: foo@example.com\n",
            *extension_value);
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
  builder->SetSubjectTLV(kSubject);

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
  builder->SetSubjectTLV(kSubject);

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
  builder->SetSubjectTLV(kSubject);

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

// TODO(crbug.com/41453265): This test suite has "2" at the end of the
// name to avoid conflicting with x509_certificate_model_nss_unittest. Should
// rename the test suite in that file to X509CertificateModelNSSTest and remove
// the 2 from here.
TEST(X509CertificateModelTest2, ProcessRawSubjectPublicKeyInfo) {
  // SEQUENCE {
  //   SEQUENCE {
  //     # rsaEncryption
  //     OBJECT_IDENTIFIER { 1.2.840.113549.1.1.1 }
  //     NULL {}
  //   }
  //   BIT_STRING {
  //     `00`
  //     SEQUENCE {
  //       INTEGER {
  //       `00e053f4f398c1143302c8a46dfeaa2af7943da66f00df3bde4c9fa3ea07d4ac`
  //       `e55b0dd1ace0edf9c5981d352de5b349971485440fdc4cd267088801a5d8a7eb`
  //       `93d16aa1f751e7847e522a7dbc6f0ed8dbb6a63ededcf5a4689644118502ed47`
  //       `12dfb86071957b6287687a445609d5b4c8f1f6c946928b68e883d5d5867123c3`
  //       `801ebf6c01c7d2a4bc406de0e3c02e3078bdaddd2566d3f5070756d7cee272c5`
  //       `257d0ce1a76f00a8daab4b54430964a4b652382fb7cc01dd1c03270347bfdfe6`
  //       `37b0ed18dc510bd47522df507b3ceb37391c9b6f087ba705ac8c43f7f1da5106`
  //       `b382453ec881739eb0a5cf7696af812cac012a4a584b1dbeff1f85c227def178`
  //       `0b`
  //       } INTEGER { 65537 }
  //     }
  //   }
  // }
  const uint8_t spki_bytes[] = {
      0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
      0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00,
      0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xe0, 0x53, 0xf4,
      0xf3, 0x98, 0xc1, 0x14, 0x33, 0x02, 0xc8, 0xa4, 0x6d, 0xfe, 0xaa, 0x2a,
      0xf7, 0x94, 0x3d, 0xa6, 0x6f, 0x00, 0xdf, 0x3b, 0xde, 0x4c, 0x9f, 0xa3,
      0xea, 0x07, 0xd4, 0xac, 0xe5, 0x5b, 0x0d, 0xd1, 0xac, 0xe0, 0xed, 0xf9,
      0xc5, 0x98, 0x1d, 0x35, 0x2d, 0xe5, 0xb3, 0x49, 0x97, 0x14, 0x85, 0x44,
      0x0f, 0xdc, 0x4c, 0xd2, 0x67, 0x08, 0x88, 0x01, 0xa5, 0xd8, 0xa7, 0xeb,
      0x93, 0xd1, 0x6a, 0xa1, 0xf7, 0x51, 0xe7, 0x84, 0x7e, 0x52, 0x2a, 0x7d,
      0xbc, 0x6f, 0x0e, 0xd8, 0xdb, 0xb6, 0xa6, 0x3e, 0xde, 0xdc, 0xf5, 0xa4,
      0x68, 0x96, 0x44, 0x11, 0x85, 0x02, 0xed, 0x47, 0x12, 0xdf, 0xb8, 0x60,
      0x71, 0x95, 0x7b, 0x62, 0x87, 0x68, 0x7a, 0x44, 0x56, 0x09, 0xd5, 0xb4,
      0xc8, 0xf1, 0xf6, 0xc9, 0x46, 0x92, 0x8b, 0x68, 0xe8, 0x83, 0xd5, 0xd5,
      0x86, 0x71, 0x23, 0xc3, 0x80, 0x1e, 0xbf, 0x6c, 0x01, 0xc7, 0xd2, 0xa4,
      0xbc, 0x40, 0x6d, 0xe0, 0xe3, 0xc0, 0x2e, 0x30, 0x78, 0xbd, 0xad, 0xdd,
      0x25, 0x66, 0xd3, 0xf5, 0x07, 0x07, 0x56, 0xd7, 0xce, 0xe2, 0x72, 0xc5,
      0x25, 0x7d, 0x0c, 0xe1, 0xa7, 0x6f, 0x00, 0xa8, 0xda, 0xab, 0x4b, 0x54,
      0x43, 0x09, 0x64, 0xa4, 0xb6, 0x52, 0x38, 0x2f, 0xb7, 0xcc, 0x01, 0xdd,
      0x1c, 0x03, 0x27, 0x03, 0x47, 0xbf, 0xdf, 0xe6, 0x37, 0xb0, 0xed, 0x18,
      0xdc, 0x51, 0x0b, 0xd4, 0x75, 0x22, 0xdf, 0x50, 0x7b, 0x3c, 0xeb, 0x37,
      0x39, 0x1c, 0x9b, 0x6f, 0x08, 0x7b, 0xa7, 0x05, 0xac, 0x8c, 0x43, 0xf7,
      0xf1, 0xda, 0x51, 0x06, 0xb3, 0x82, 0x45, 0x3e, 0xc8, 0x81, 0x73, 0x9e,
      0xb0, 0xa5, 0xcf, 0x76, 0x96, 0xaf, 0x81, 0x2c, 0xac, 0x01, 0x2a, 0x4a,
      0x58, 0x4b, 0x1d, 0xbe, 0xff, 0x1f, 0x85, 0xc2, 0x27, 0xde, 0xf1, 0x78,
      0x0b, 0x02, 0x03, 0x01, 0x00, 0x01};
  EXPECT_EQ(
      "Modulus (2048 bits):\n"
      "  E0 53 F4 F3 98 C1 14 33 02 C8 A4 6D FE AA 2A F7\n"
      "94 3D A6 6F 00 DF 3B DE 4C 9F A3 EA 07 D4 AC E5\n"
      "5B 0D D1 AC E0 ED F9 C5 98 1D 35 2D E5 B3 49 97\n"
      "14 85 44 0F DC 4C D2 67 08 88 01 A5 D8 A7 EB 93\n"
      "D1 6A A1 F7 51 E7 84 7E 52 2A 7D BC 6F 0E D8 DB\n"
      "B6 A6 3E DE DC F5 A4 68 96 44 11 85 02 ED 47 12\n"
      "DF B8 60 71 95 7B 62 87 68 7A 44 56 09 D5 B4 C8\n"
      "F1 F6 C9 46 92 8B 68 E8 83 D5 D5 86 71 23 C3 80\n"
      "1E BF 6C 01 C7 D2 A4 BC 40 6D E0 E3 C0 2E 30 78\n"
      "BD AD DD 25 66 D3 F5 07 07 56 D7 CE E2 72 C5 25\n"
      "7D 0C E1 A7 6F 00 A8 DA AB 4B 54 43 09 64 A4 B6\n"
      "52 38 2F B7 CC 01 DD 1C 03 27 03 47 BF DF E6 37\n"
      "B0 ED 18 DC 51 0B D4 75 22 DF 50 7B 3C EB 37 39\n"
      "1C 9B 6F 08 7B A7 05 AC 8C 43 F7 F1 DA 51 06 B3\n"
      "82 45 3E C8 81 73 9E B0 A5 CF 76 96 AF 81 2C AC\n"
      "01 2A 4A 58 4B 1D BE FF 1F 85 C2 27 DE F1 78 0B\n"
      "\n"
      "  Public Exponent (17 bits):\n"
      "  01 00 01",
      x509_certificate_model::ProcessRawSubjectPublicKeyInfo(spki_bytes));
}
