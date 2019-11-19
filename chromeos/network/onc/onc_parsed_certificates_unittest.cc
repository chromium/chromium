// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_parsed_certificates.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "net/cert/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace onc {

class OncParsedCertificatesTest : public testing::Test {
 public:
  OncParsedCertificatesTest() = default;
  ~OncParsedCertificatesTest() override = default;

 protected:
  bool ReadFromJSON(
      base::StringPiece onc_certificates_json,
      std::unique_ptr<OncParsedCertificates>* out_onc_parsed_certificates) {
    std::unique_ptr<base::Value> onc_certificates =
        base::JSONReader::ReadDeprecated(onc_certificates_json);
    if (!onc_certificates)
      return false;
    *out_onc_parsed_certificates =
        std::make_unique<OncParsedCertificates>(*onc_certificates);
    return true;
  }
};

TEST_F(OncParsedCertificatesTest, ClientCert) {
  const char onc_certificates_json[] = R"(
      [
        { "GUID": "{f998f760-272b-6939-4c2beffe428697ac}",
          "PKCS12": "YWJj",
          "Type": "Client" }
      ])";

  std::unique_ptr<OncParsedCertificates> onc_parsed_certificates;
  ASSERT_TRUE(ReadFromJSON(onc_certificates_json, &onc_parsed_certificates));

  EXPECT_FALSE(onc_parsed_certificates->has_error());
  EXPECT_EQ(0u,
            onc_parsed_certificates->server_or_authority_certificates().size());
  ASSERT_EQ(1u, onc_parsed_certificates->client_certificates().size());

  const OncParsedCertificates::ClientCertificate& client_cert =
      onc_parsed_certificates->client_certificates()[0];

  EXPECT_EQ("{f998f760-272b-6939-4c2beffe428697ac}", client_cert.guid());
  // YWJj base64-decoded is abc
  EXPECT_EQ("abc", client_cert.pkcs12_data());
}

TEST_F(OncParsedCertificatesTest, ClientCertWithNewLines) {
  const char onc_certificates_json[] = R"(
      [
        { "GUID": "{f998f760-272b-6939-4c2beffe428697ac}",
          "PKCS12": "YW\nJ\n\n\nj",
          "Type": "Client" }
      ])";

  std::unique_ptr<OncParsedCertificates> onc_parsed_certificates;
  ASSERT_TRUE(ReadFromJSON(onc_certificates_json, &onc_parsed_certificates));

  EXPECT_FALSE(onc_parsed_certificates->has_error());
  EXPECT_EQ(0u,
            onc_parsed_certificates->server_or_authority_certificates().size());
  ASSERT_EQ(1u, onc_parsed_certificates->client_certificates().size());

  const OncParsedCertificates::ClientCertificate& client_cert =
      onc_parsed_certificates->client_certificates()[0];

  EXPECT_EQ("{f998f760-272b-6939-4c2beffe428697ac}", client_cert.guid());
  // YWJj base64-decoded is abc
  EXPECT_EQ("abc", client_cert.pkcs12_data());
}

TEST_F(OncParsedCertificatesTest, ClientCertAndError) {
  const char  onc_certificates_json[] = R"(
      [
        { "GUID": "{good-client-cert}",
          "PKCS12": "YWJj",
          "Type": "Client" },
        { "GUID": "{bad-client-cert}",
          "PKCS12": "!!!",
          "Type": "Client" }
      ])";

  std::unique_ptr<OncParsedCertificates> onc_parsed_certificates;
  ASSERT_TRUE(ReadFromJSON(onc_certificates_json, &onc_parsed_certificates));

  EXPECT_TRUE(onc_parsed_certificates->has_error());
  EXPECT_EQ(0u,
            onc_parsed_certificates->server_or_authority_certificates().size());
  ASSERT_EQ(1u, onc_parsed_certificates->client_certificates().size());

  const OncParsedCertificates::ClientCertificate& client_cert =
      onc_parsed_certificates->client_certificates()[0];

  EXPECT_EQ("{good-client-cert}", client_cert.guid());
}

TEST_F(OncParsedCertificatesTest, AuthorityCerts) {
  const char  onc_certificates_json[] = R"(
      [
        { "GUID": "{trusted-cert}",
          "TrustBits": [
             "Web"
          ],
          "Type": "Authority",
          "X509": "-----BEGIN CERTIFICATE-----\n
      MIIC8zCCAdugAwIBAgIJALF9qhLor0+aMA0GCSqGSIb3DQEBBQUAMBcxFTATBgNV\n
      BAMMDFRlc3QgUm9vdCBDQTAeFw0xNDA4MTQwMzA1MjlaFw0yNDA4MTEwMzA1Mjla\n
      MBcxFTATBgNVBAMMDFRlc3QgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEP\n
      ADCCAQoCggEBALZJQeNCAVGofzx6cdP7zZE1F4QajvY2x9FwHfqG8267dm/oMi43\n
      /TiSPWjkin1CMxRGG9wE9pFuVEDECgn97C1i4l7huiycwbFgTNrH+CJcgiBlQh5W\n
      d3VP65AsSupXDiKNbJWsEerM1+72cA0J3aY1YV3Jdm2w8h6/MIbYd1I2lZcO0UbF\n
      7YE9G7DyYZU8wUA4719dumGf7yucn4WJdHBj1XboNX7OAeHzERGQHA31/Y3OEGyt\n
      fFUaIW/XLfR4FeovOL2RnjwdB0b1Q8GCi68SU2UZimlpZgay2gv6KgChKhWESfEB\n
      v5swBtAVoB+dUZFH4VNf717swmF5whSfxOMCAwEAAaNCMEAwDwYDVR0TAQH/BAUw\n
      AwEB/zAdBgNVHQ4EFgQUvPcw0TzA8nn675/JbFyT84poq4MwDgYDVR0PAQH/BAQD\n
      AgEGMA0GCSqGSIb3DQEBBQUAA4IBAQBXByn7f+j/sObYWGrDkKE4HLTzaLHs6Ikj\n
      JNeo8iHDYOSkSVwAv9/HgniAKxj3rd3QYl6nsMzwqrTOcBJZZWd2BQAYmv/EKhfj\n
      8VXYvlxe68rLU4cQ1QkyNqdeQfRT2n5WYNJ+TpqlCF9ddennMMsi6e8ZSYOlI6H4\n
      YEzlNtU5eBjxXr/OqgtTgSx4qQpr2xMQIRR/G3A9iRpAigYsXVAZYvnHRYnyPWYF\n
      PX11W1UegEJyoZp8bQp09u6mIWw6mPt3gl/ya1bm3ZuOUPDGrv3qpgUHqSYGVrOy\n
      2bI3oCE+eQYfuVG+9LFJTZC1M+UOx15bQMVqBNFDepRqpE9h/ILg\n
      -----END CERTIFICATE-----" },
        { "GUID": "{untrusted-cert}",
          "Type": "Authority",
          "X509": "-----BEGIN CERTIFICATE-----\n
      MIIDvzCCAqegAwIBAgIBAzANBgkqhkiG9w0BAQsFADBjMQswCQYDVQQGEwJVUzET\n
      MBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNTW91bnRhaW4gVmlldzEQMA4G\n
      A1UECgwHVGVzdCBDQTEVMBMGA1UEAwwMVGVzdCBSb290IENBMB4XDTE3MDYwNTE3\n
      MTA0NloXDTI3MDYwMzE3MTA0NlowYDELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNh\n
      bGlmb3JuaWExFjAUBgNVBAcMDU1vdW50YWluIFZpZXcxEDAOBgNVBAoMB1Rlc3Qg\n
      Q0ExEjAQBgNVBAMMCTEyNy4wLjAuMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCC\n
      AQoCggEBALS/0pcz5RNbd2W9cxp1KJtHWea3MOhGM21YW9ofCv/k5C3yHfiJ6GQu\n
      9sPN16OO1/fN59gOEMPnVtL85ebTTuL/gk0YY4ewo97a7wo3e6y1t0PO8gc53xTp\n
      w6RBPn5oRzSbe2HEGOYTzrO0puC6A+7k6+eq9G2+l1uqBpdQAdB4uNaSsOTiuUOI\n
      ta4UZH1ScNQFHAkl1eJPyaiC20Exw75EbwvU/b/B7tlivzuPtQDI0d9dShOtceRL\n
      X9HZckyD2JNAv2zNL2YOBNa5QygkySX9WXD+PfKpCk7Cm8TenldeXRYl5ni2REkp\n
      nfa/dPuF1g3xZVjyK9aPEEnIAC2I4i0CAwEAAaOBgDB+MAwGA1UdEwEB/wQCMAAw\n
      HQYDVR0OBBYEFODc4C8HiHQ6n9Mwo3GK+dal5aZTMB8GA1UdIwQYMBaAFJsmC4qY\n
      qbsduR8c4xpAM+2OF4irMB0GA1UdJQQWMBQGCCsGAQUFBwMBBggrBgEFBQcDAjAP\n
      BgNVHREECDAGhwR/AAABMA0GCSqGSIb3DQEBCwUAA4IBAQB6FEQuUDRcC5jkX3aZ\n
      uuTeZEqMVL7JXgvgFqzXsPb8zIdmxr/tEDfwXx2qDf2Dpxts7Fq4vqUwimK4qV3K\n
      7heLnWV2+FBvV1eeSfZ7AQj+SURkdlyo42r41+t13QUf+Z0ftR9266LSWLKrukeI\n
      Mxk73hOkm/u8enhTd00dy/FN9dOFBFHseVMspWNxIkdRILgOmiyfQNRgxNYdOf0e\n
      EfELR8Hn6WjZ8wAbvO4p7RTrzu1c/RZ0M+NLkID56Brbl70GC2h5681LPwAOaZ7/\n
      mWQ5kekSyJjmLfF12b+h9RVAt5MrXZgk2vNujssgGf4nbWh4KZyQ6qrs778ZdDLm\n
      yfUn\n
      -----END CERTIFICATE-----" }
      ])";

  std::unique_ptr<OncParsedCertificates> onc_parsed_certificates;
  ASSERT_TRUE(ReadFromJSON(onc_certificates_json, &onc_parsed_certificates));

  EXPECT_FALSE(onc_parsed_certificates->has_error());
  EXPECT_EQ(2u,
            onc_parsed_certificates->server_or_authority_certificates().size());
  EXPECT_EQ(0u, onc_parsed_certificates->client_certificates().size());

  const OncParsedCertificates::ServerOrAuthorityCertificate&
      trusted_authority_cert =
          onc_parsed_certificates->server_or_authority_certificates()[0];
  EXPECT_EQ(
      OncParsedCertificates::ServerOrAuthorityCertificate::Type::kAuthority,
      trusted_authority_cert.type());
  EXPECT_EQ(CertificateScope::Default(), trusted_authority_cert.scope());
  EXPECT_EQ("{trusted-cert}", trusted_authority_cert.guid());
  EXPECT_TRUE(trusted_authority_cert.web_trust_requested());
  EXPECT_EQ("Test Root CA",
            trusted_authority_cert.certificate()->subject().common_name);

  const OncParsedCertificates::ServerOrAuthorityCertificate&
      untrusted_authority_cert =
          onc_parsed_certificates->server_or_authority_certificates()[1];
  EXPECT_EQ(
      OncParsedCertificates::ServerOrAuthorityCertificate::Type::kAuthority,
      trusted_authority_cert.type());
  EXPECT_EQ(CertificateScope::Default(), trusted_authority_cert.scope());
  EXPECT_EQ("{untrusted-cert}", untrusted_authority_cert.guid());
  EXPECT_FALSE(untrusted_authority_cert.web_trust_requested());
  EXPECT_EQ("127.0.0.1",
            untrusted_authority_cert.certificate()->subject().common_name);
}

TEST_F(OncParsedCertificatesTest, AuthorityCertsScope) {
  const char onc_certificates_json[] = R"(
      [
        { "GUID": "{extension-scoped-cert}",
          "Type": "Authority",
          "TrustBits": [
             "Web"
          ],
          "Scope": {
            "Type": "Extension",
            "Id": "fake-extension-id"
          },
          "X509": "-----BEGIN CERTIFICATE-----\n
      MIIC8zCCAdugAwIBAgIJALF9qhLor0+aMA0GCSqGSIb3DQEBBQUAMBcxFTATBgNV\n
      BAMMDFRlc3QgUm9vdCBDQTAeFw0xNDA4MTQwMzA1MjlaFw0yNDA4MTEwMzA1Mjla\n
      MBcxFTATBgNVBAMMDFRlc3QgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEP\n
      ADCCAQoCggEBALZJQeNCAVGofzx6cdP7zZE1F4QajvY2x9FwHfqG8267dm/oMi43\n
      /TiSPWjkin1CMxRGG9wE9pFuVEDECgn97C1i4l7huiycwbFgTNrH+CJcgiBlQh5W\n
      d3VP65AsSupXDiKNbJWsEerM1+72cA0J3aY1YV3Jdm2w8h6/MIbYd1I2lZcO0UbF\n
      7YE9G7DyYZU8wUA4719dumGf7yucn4WJdHBj1XboNX7OAeHzERGQHA31/Y3OEGyt\n
      fFUaIW/XLfR4FeovOL2RnjwdB0b1Q8GCi68SU2UZimlpZgay2gv6KgChKhWESfEB\n
      v5swBtAVoB+dUZFH4VNf717swmF5whSfxOMCAwEAAaNCMEAwDwYDVR0TAQH/BAUw\n
      AwEB/zAdBgNVHQ4EFgQUvPcw0TzA8nn675/JbFyT84poq4MwDgYDVR0PAQH/BAQD\n
      AgEGMA0GCSqGSIb3DQEBBQUAA4IBAQBXByn7f+j/sObYWGrDkKE4HLTzaLHs6Ikj\n
      JNeo8iHDYOSkSVwAv9/HgniAKxj3rd3QYl6nsMzwqrTOcBJZZWd2BQAYmv/EKhfj\n
      8VXYvlxe68rLU4cQ1QkyNqdeQfRT2n5WYNJ+TpqlCF9ddennMMsi6e8ZSYOlI6H4\n
      YEzlNtU5eBjxXr/OqgtTgSx4qQpr2xMQIRR/G3A9iRpAigYsXVAZYvnHRYnyPWYF\n
      PX11W1UegEJyoZp8bQp09u6mIWw6mPt3gl/ya1bm3ZuOUPDGrv3qpgUHqSYGVrOy\n
      2bI3oCE+eQYfuVG+9LFJTZC1M+UOx15bQMVqBNFDepRqpE9h/ILg\n
      -----END CERTIFICATE-----" }
      ])";

  std::unique_ptr<OncParsedCertificates> onc_parsed_certificates;
  ASSERT_TRUE(ReadFromJSON(onc_certificates_json, &onc_parsed_certificates));

  EXPECT_FALSE(onc_parsed_certificates->has_error());
  ASSERT_EQ(1u,
            onc_parsed_certificates->server_or_authority_certificates().size());

  const OncParsedCertificates::ServerOrAuthorityCertificate& authority_cert =
      onc_parsed_certificates->server_or_authority_certificates()[0];
  EXPECT_EQ(
      OncParsedCertificates::ServerOrAuthorityCertificate::Type::kAuthority,
      authority_cert.type());
  EXPECT_EQ(CertificateScope::ForExtension("fake-extension-id"),
            authority_cert.scope());
  EXPECT_EQ("{extension-scoped-cert}", authority_cert.guid());
  EXPECT_TRUE(authority_cert.web_trust_requested());
  EXPECT_EQ("Test Root CA",
            authority_cert.certificate()->subject().common_name);
}

TEST_F(OncParsedCertificatesTest, UnknownTrustBitsIgnored) {
  const char onc_certificates_json[] =R"(
      [
        { "GUID": "{trusted-cert}",
          "TrustBits": [
             "Unknown1",
             "Web",
             "Unknown2"
          ],
          "Type": "Authority",
          "X509": "-----BEGIN CERTIFICATE-----\n
      MIIC8zCCAdugAwIBAgIJALF9qhLor0+aMA0GCSqGSIb3DQEBBQUAMBcxFTATBgNV\n
      BAMMDFRlc3QgUm9vdCBDQTAeFw0xNDA4MTQwMzA1MjlaFw0yNDA4MTEwMzA1Mjla\n
      MBcxFTATBgNVBAMMDFRlc3QgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEP\n
      ADCCAQoCggEBALZJQeNCAVGofzx6cdP7zZE1F4QajvY2x9FwHfqG8267dm/oMi43\n
      /TiSPWjkin1CMxRGG9wE9pFuVEDECgn97C1i4l7huiycwbFgTNrH+CJcgiBlQh5W\n
      d3VP65AsSupXDiKNbJWsEerM1+72cA0J3aY1YV3Jdm2w8h6/MIbYd1I2lZcO0UbF\n
      7YE9G7DyYZU8wUA4719dumGf7yucn4WJdHBj1XboNX7OAeHzERGQHA31/Y3OEGyt\n
      fFUaIW/XLfR4FeovOL2RnjwdB0b1Q8GCi68SU2UZimlpZgay2gv6KgChKhWESfEB\n
      v5swBtAVoB+dUZFH4VNf717swmF5whSfxOMCAwEAAaNCMEAwDwYDVR0TAQH/BAUw\n
      AwEB/zAdBgNVHQ4EFgQUvPcw0TzA8nn675/JbFyT84poq4MwDgYDVR0PAQH/BAQD\n
      AgEGMA0GCSqGSIb3DQEBBQUAA4IBAQBXByn7f+j/sObYWGrDkKE4HLTzaLHs6Ikj\n
      JNeo8iHDYOSkSVwAv9/HgniAKxj3rd3QYl6nsMzwqrTOcBJZZWd2BQAYmv/EKhfj\n
      8VXYvlxe68rLU4cQ1QkyNqdeQfRT2n5WYNJ+TpqlCF9ddennMMsi6e8ZSYOlI6H4\n
      YEzlNtU5eBjxXr/OqgtTgSx4qQpr2xMQIRR/G3A9iRpAigYsXVAZYvnHRYnyPWYF\n
      PX11W1UegEJyoZp8bQp09u6mIWw6mPt3gl/ya1bm3ZuOUPDGrv3qpgUHqSYGVrOy\n
      2bI3oCE+eQYfuVG+9LFJTZC1M+UOx15bQMVqBNFDepRqpE9h/ILg\n
      -----END CERTIFICATE-----" }
      ])";

  std::unique_ptr<OncParsedCertificates> onc_parsed_certificates;
  ASSERT_TRUE(ReadFromJSON(onc_certificates_json, &onc_parsed_certificates));

  EXPECT_FALSE(onc_parsed_certificates->has_error());
  ASSERT_EQ(1u,
            onc_parsed_certificates->server_or_authority_certificates().size());
  EXPECT_EQ(0u, onc_parsed_certificates->client_certificates().size());

  const OncParsedCertificates::ServerOrAuthorityCertificate&
      trusted_authority_cert =
          onc_parsed_certificates->server_or_authority_certificates()[0];
  EXPECT_EQ(
      OncParsedCertificates::ServerOrAuthorityCertificate::Type::kAuthority,
      trusted_authority_cert.type());
  EXPECT_EQ("{trusted-cert}", trusted_authority_cert.guid());
  EXPECT_TRUE(trusted_authority_cert.web_trust_requested());
  EXPECT_EQ("Test Root CA",
            trusted_authority_cert.certificate()->issuer().common_name);
}

TEST_F(OncParsedCertificatesTest, ServerCertAndError) {
  const char onc_certificates_json[] = R"(
      [
        { "GUID": "{good-server-cert}",
          "Type": "Server",
          "X509": "leading junk \n
      -----BEGIN CERTIFICATE-----  \n
      MIICWDCCAcECAxAAATANBgkqhkiG9w0BAQQFADCBkzEVMBMGA1UEChMMR29vZ2xlLCBJbm\n
      MuMREwDwYDVQQLEwhDaHJvbWVPUzEiMCAGCSqGSIb3DQEJARYTZ3NwZW5jZXJAZ29vZ2xl\n
      LmNvbTEaMBgGA1UEBxMRTW91bnRhaW4gVmlldywgQ0ExCzAJBgNVBAgTAkNBMQswCQYDVQ\n
      QGEwJVUzENMAsGA1UEAxMEbG1hbzAeFw0xMTAzMTYyMzQ5MzhaFw0xMjAzMTUyMzQ5Mzha\n
      MFMxCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTEVMBMGA1UEChMMR29vZ2xlLCBJbmMuMR\n
      EwDwYDVQQLEwhDaHJvbWVPUzENMAsGA1UEAxMEbG1hbzCBnzANBgkqhkiG9w0BAQEFAAOB\n
      jQAwgYkCgYEA31WiJ9LvprrhKtDlW0RdLFAO7Qjkvs+sG6j2Vp2aBSrlhALG/0BVHUhWi4\n
      F/HHJho+ncLHAg5AGO0sdAjYUdQG6tfPqjLsIALtoKEZZdFe/JhmqOEaxWsSdu2S2RdPgC\n
      QOsP79EH58gXwu2gejCkJDmU22WL4YLuqOc17nxbDC8CAwEAATANBgkqhkiG9w0BAQQFAA\n
      OBgQCv4vMD+PMlfnftu4/6Yf/oMLE8yCOqZTQ/dWCxB9PiJnOefiBeSzSZE6Uv3G7qnblZ\n
      PVZaFeJMd+ostt0viCyPucFsFgLMyyoV1dMVPVwJT5Iq1AHehWXnTBbxUK9wioA5jOEKdr\n
      oKjuSSsg/Q8Wx6cpJmttQz5olGPgstmACRWA==\n
      -----END CERTIFICATE-----    \n
      trailing junk" },
        { "GUID": "{bad-server-cert}",
          "Type": "Server",
          "X509": "leading junk \n
      -----BEGIN CERTIFICATE-----  \n
      !!!! 
      MIICWDCCAcECAxAAATANBgkqhkiG9w0BAQQFADCBkzEVMBMGA1UEChMMR29vZ2xlLCBJbm\n
      MuMREwDwYDVQQLEwhDaHJvbWVPUzEiMCAGCSqGSIb3DQEJARYTZ3NwZW5jZXJAZ29vZ2xl\n
      LmNvbTEaMBgGA1UEBxMRTW91bnRhaW4gVmlldywgQ0ExCzAJBgNVBAgTAkNBMQswCQYDVQ\n
      QGEwJVUzENMAsGA1UEAxMEbG1hbzAeFw0xMTAzMTYyMzQ5MzhaFw0xMjAzMTUyMzQ5Mzha\n
      MFMxCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTEVMBMGA1UEChMMR29vZ2xlLCBJbmMuMR\n
      EwDwYDVQQLEwhDaHJvbWVPUzENMAsGA1UEAxMEbG1hbzCBnzANBgkqhkiG9w0BAQEFAAOB\n
      jQAwgYkCgYEA31WiJ9LvprrhKtDlW0RdLFAO7Qjkvs+sG6j2Vp2aBSrlhALG/0BVHUhWi4\n
      F/HHJho+ncLHAg5AGO0sdAjYUdQG6tfPqjLsIALtoKEZZdFe/JhmqOEaxWsSdu2S2RdPgC\n
      QOsP79EH58gXwu2gejCkJDmU22WL4YLuqOc17nxbDC8CAwEAATANBgkqhkiG9w0BAQQFAA\n
      OBgQCv4vMD+PMlfnftu4/6Yf/oMLE8yCOqZTQ/dWCxB9PiJnOefiBeSzSZE6Uv3G7qnblZ\n
      PVZaFeJMd+ostt0viCyPucFsFgLMyyoV1dMVPVwJT5Iq1AHehWXnTBbxUK9wioA5jOEKdr\n
      oKjuSSsg/Q8Wx6cpJmttQz5olGPgstmACRWA==\n
      -----END CERTIFICATE-----    \n
      trailing junk" }
      ])";

  std::unique_ptr<OncParsedCertificates> onc_parsed_certificates;
  ASSERT_TRUE(ReadFromJSON(onc_certificates_json, &onc_parsed_certificates));

  EXPECT_TRUE(onc_parsed_certificates->has_error());
  EXPECT_EQ(1u,
            onc_parsed_certificates->server_or_authority_certificates().size());
  EXPECT_EQ(0u, onc_parsed_certificates->client_certificates().size());

  const OncParsedCertificates::ServerOrAuthorityCertificate& server_cert =
      onc_parsed_certificates->server_or_authority_certificates()[0];
  EXPECT_EQ(OncParsedCertificates::ServerOrAuthorityCertificate::Type::kServer,
            server_cert.type());
  EXPECT_EQ("{good-server-cert}", server_cert.guid());
  EXPECT_FALSE(server_cert.web_trust_requested());
  EXPECT_EQ("lmao", server_cert.certificate()->issuer().common_name);
}

TEST_F(OncParsedCertificatesTest, EqualityChecks) {
  const char onc_certificates_json[] = R"(
      [
        { "GUID": "{f998f760-272b-6939-4c2beffe428697ac}",
          "PKCS12": "YWJj",
          "Type": "Client" },
        { "GUID": "{authority-cert}",
          "TrustBits": [
             "Web"
          ],
          "Type": "Authority",
          "X509": "-----BEGIN CERTIFICATE-----\n
      MIIC8zCCAdugAwIBAgIJALF9qhLor0+aMA0GCSqGSIb3DQEBBQUAMBcxFTATBgNV\n
      BAMMDFRlc3QgUm9vdCBDQTAeFw0xNDA4MTQwMzA1MjlaFw0yNDA4MTEwMzA1Mjla\n
      MBcxFTATBgNVBAMMDFRlc3QgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEP\n
      ADCCAQoCggEBALZJQeNCAVGofzx6cdP7zZE1F4QajvY2x9FwHfqG8267dm/oMi43\n
      /TiSPWjkin1CMxRGG9wE9pFuVEDECgn97C1i4l7huiycwbFgTNrH+CJcgiBlQh5W\n
      d3VP65AsSupXDiKNbJWsEerM1+72cA0J3aY1YV3Jdm2w8h6/MIbYd1I2lZcO0UbF\n
      7YE9G7DyYZU8wUA4719dumGf7yucn4WJdHBj1XboNX7OAeHzERGQHA31/Y3OEGyt\n
      fFUaIW/XLfR4FeovOL2RnjwdB0b1Q8GCi68SU2UZimlpZgay2gv6KgChKhWESfEB\n
      v5swBtAVoB+dUZFH4VNf717swmF5whSfxOMCAwEAAaNCMEAwDwYDVR0TAQH/BAUw\n
      AwEB/zAdBgNVHQ4EFgQUvPcw0TzA8nn675/JbFyT84poq4MwDgYDVR0PAQH/BAQD\n
      AgEGMA0GCSqGSIb3DQEBBQUAA4IBAQBXByn7f+j/sObYWGrDkKE4HLTzaLHs6Ikj\n
      JNeo8iHDYOSkSVwAv9/HgniAKxj3rd3QYl6nsMzwqrTOcBJZZWd2BQAYmv/EKhfj\n
      8VXYvlxe68rLU4cQ1QkyNqdeQfRT2n5WYNJ+TpqlCF9ddennMMsi6e8ZSYOlI6H4\n
      YEzlNtU5eBjxXr/OqgtTgSx4qQpr2xMQIRR/G3A9iRpAigYsXVAZYvnHRYnyPWYF\n
      PX11W1UegEJyoZp8bQp09u6mIWw6mPt3gl/ya1bm3ZuOUPDGrv3qpgUHqSYGVrOy\n
      2bI3oCE+eQYfuVG+9LFJTZC1M+UOx15bQMVqBNFDepRqpE9h/ILg\n
      -----END CERTIFICATE-----" }
      ])";

  std::unique_ptr<base::Value> onc_certificates =
      base::JSONReader::ReadDeprecated(onc_certificates_json);
  ASSERT_TRUE(onc_certificates);

  OncParsedCertificates master(*onc_certificates);
  EXPECT_EQ(master.server_or_authority_certificates(),
            master.server_or_authority_certificates());
  EXPECT_EQ(master.client_certificates(), master.client_certificates());

  // Mangle the TrustBits part and assume that authorities will not be equal
  // anymore.
  {
    base::Value authority_web_trust_mangled = onc_certificates->Clone();
    base::Value* trust_bits =
        authority_web_trust_mangled.GetList()[1].FindKeyOfType(
            "TrustBits", base::Value::Type::LIST);
    ASSERT_TRUE(trust_bits);
    trust_bits->GetList()[0] = base::Value("UnknownTrustBit");

    OncParsedCertificates parsed_authority_web_trust_mangled(
        authority_web_trust_mangled);
    EXPECT_FALSE(parsed_authority_web_trust_mangled.has_error());
    EXPECT_NE(
        master.server_or_authority_certificates(),
        parsed_authority_web_trust_mangled.server_or_authority_certificates());
    EXPECT_EQ(master.client_certificates(),
              parsed_authority_web_trust_mangled.client_certificates());
  }

  // Mangle the guid part of an authority certificate.
  {
    base::Value authority_guid_mangled = onc_certificates->Clone();
    authority_guid_mangled.GetList()[1].SetKey("GUID",
                                               base::Value("otherguid"));

    OncParsedCertificates parsed_authority_guid_mangled(authority_guid_mangled);
    EXPECT_FALSE(parsed_authority_guid_mangled.has_error());
    EXPECT_NE(master.server_or_authority_certificates(),
              parsed_authority_guid_mangled.server_or_authority_certificates());
    EXPECT_EQ(master.client_certificates(),
              parsed_authority_guid_mangled.client_certificates());
  }

  // Mangle the type part of an authority certificate.
  {
    base::Value authority_type_mangled = onc_certificates->Clone();
    authority_type_mangled.GetList()[1].SetKey("Type", base::Value("Server"));

    OncParsedCertificates parsed_authority_type_mangled(authority_type_mangled);
    EXPECT_FALSE(parsed_authority_type_mangled.has_error());
    EXPECT_NE(master.server_or_authority_certificates(),
              parsed_authority_type_mangled.server_or_authority_certificates());
    EXPECT_EQ(master.client_certificates(),
              parsed_authority_type_mangled.client_certificates());
  }

  // Mangle the X509 payload an authority certificate.
  {
    base::Value authority_x509_mangled = onc_certificates->Clone();
    authority_x509_mangled.GetList()[1].SetKey(
        "X509", base::Value(R"(
                            -----BEGIN CERTIFICATE-----
                            MIICWDCCAcECAxAAATANBgkqhkiG9w0BAQQFADCBkzEVMBMGA1
                            UEChMMR29vZ2xlLCBJbm
                            MuMREwDwYDVQQLEwhDaHJvbWVPUzEiMCAGCSqGSIb3DQEJARYT
                            Z3NwZW5jZXJAZ29vZ2xl
                            LmNvbTEaMBgGA1UEBxMRTW91bnRhaW4gVmlldywgQ0ExCzAJBg
                            NVBAgTAkNBMQswCQYDVQ                
                            QGEwJVUzENMAsGA1UEAxMEbG1hbzAeFw0xMTAzMTYyMzQ5Mzha
                            Fw0xMjAzMTUyMzQ5Mzha                
                            MFMxCzAJBgNVBAYTAlVTMQswCQYDVQQIEwJDQTEVMBMGA1UECh
                            MMR29vZ2xlLCBJbmMuMR                
                            EwDwYDVQQLEwhDaHJvbWVPUzENMAsGA1UEAxMEbG1hbzCBnzAN
                            BgkqhkiG9w0BAQEFAAOB                
                            jQAwgYkCgYEA31WiJ9LvprrhKtDlW0RdLFAO7Qjkvs+
                            sG6j2Vp2aBSrlhALG/0BVHUhWi4                
                            F/HHJho+ncLHAg5AGO0sdAjYUdQG6tfPqjLsIALtoKEZZdFe/
                            JhmqOEaxWsSdu2S2RdPgC                
                            QOsP79EH58gXwu2gejCkJDmU22WL4YLuqOc17nxbDC8CAwEAAT
                            ANBgkqhkiG9w0BAQQFAA                
                            OBgQCv4vMD+PMlfnftu4/6Yf/oMLE8yCOqZTQ/
                            dWCxB9PiJnOefiBeSzSZE6Uv3G7qnblZ                
                            PVZaFeJMd+
                            ostt0viCyPucFsFgLMyyoV1dMVPVwJT5Iq1AHehWXnTBbxUK9w
                            ioA5jOEKdr                
                            oKjuSSsg/Q8Wx6cpJmttQz5olGPgstmACRWA==                
                            -----END CERTIFICATE-----                    )"));

    OncParsedCertificates parsed_authority_x509_mangled(authority_x509_mangled);
    EXPECT_FALSE(parsed_authority_x509_mangled.has_error());
    EXPECT_NE(master.server_or_authority_certificates(),
              parsed_authority_x509_mangled.server_or_authority_certificates());
    EXPECT_EQ(master.client_certificates(),
              parsed_authority_x509_mangled.client_certificates());
  }

  // Mangle the GUID of a client certificate.
  {
    base::Value client_guid_mangled = onc_certificates->Clone();
    client_guid_mangled.GetList()[0].SetKey("GUID", base::Value("other-guid"));

    OncParsedCertificates parsed_client_guid_mangled(client_guid_mangled);
    EXPECT_FALSE(parsed_client_guid_mangled.has_error());
    EXPECT_EQ(master.server_or_authority_certificates(),
              parsed_client_guid_mangled.server_or_authority_certificates());
    EXPECT_NE(master.client_certificates(),
              parsed_client_guid_mangled.client_certificates());
  }

  // Mangle the PKCS12 payload of a client certificate.
  {
    base::Value client_pkcs12_mangled = onc_certificates->Clone();
    client_pkcs12_mangled.GetList()[0].SetKey("PKCS12", base::Value("YQ=="));

    OncParsedCertificates parsed_client_pkcs12_mangled(client_pkcs12_mangled);
    EXPECT_FALSE(parsed_client_pkcs12_mangled.has_error());
    EXPECT_EQ(master.server_or_authority_certificates(),
              parsed_client_pkcs12_mangled.server_or_authority_certificates());
    EXPECT_NE(master.client_certificates(),
              parsed_client_pkcs12_mangled.client_certificates());
  }
}

}  // namespace onc
}  // namespace chromeos
