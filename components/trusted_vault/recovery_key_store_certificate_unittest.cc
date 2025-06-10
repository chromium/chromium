// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/trusted_vault/recovery_key_store_certificate.h"

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/test/recovery_key_store_certificate_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace trusted_vault {

namespace {

using test_certs::GetCertificate;
using test_certs::GetEndpoint1;
using test_certs::GetEndpoint2;
using test_certs::GetEndpoint3;
using test_certs::GetIntermediate1;
using test_certs::GetIntermediate2;
using test_certs::GetSignature;
using test_certs::kCertXml;
using test_certs::kEndpoint1PublicKey;
using test_certs::kEndpoint2PublicKey;
using test_certs::kEndpoint3PublicKey;
using test_certs::kSigXml;
using test_certs::kValidCertificateDate;

class RecoveryKeyStoreCertificateTest : public testing::Test {};

TEST_F(RecoveryKeyStoreCertificateTest, Internals_ParseValidCertXml) {
  auto result = internal::ParseRecoveryKeyStoreCertXML(kCertXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->endpoints,
              testing::UnorderedElementsAre(GetEndpoint1(), GetEndpoint2(),
                                            GetEndpoint3()));
  EXPECT_THAT(
      result->intermediates,
      testing::UnorderedElementsAre(GetIntermediate1(), GetIntermediate2()));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_CertXmlWithWrongRootTag) {
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreCertXML("<not-cert></not-cert>"));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_CertXmlIgnoresEmptyCerts) {
  static constexpr std::string_view kTestXml =
      R"(<certificate>
  <intermediates>
    <cert></cert>
    <cert>intermediate</cert>
  </intermediates>
  <endpoints>
    <cert>endpoint</cert>
    <cert></cert>
  </endpoints>
</certificate>)";
  auto result = internal::ParseRecoveryKeyStoreCertXML(kTestXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->endpoints, testing::ElementsAre("endpoint"));
  EXPECT_THAT(result->intermediates, testing::ElementsAre("intermediate"));
}

TEST_F(RecoveryKeyStoreCertificateTest,
       Internals_CertXmlIgnoresUnrecognizedTags) {
  static constexpr std::string_view kTestXml =
      R"(<certificate>
  <something-else></something-else>
  <another-thing/>
  <intermediates>
    <not-cert></not-cert>
    <cert>intermediate</cert>
    <also-not-cert/>
  </intermediates>
  <endpoints>
    <not-cert></not-cert>
    <cert>endpoint</cert>
    <also-not-cert/>
  </endpoints>
  <something-else></something-else>
  <another-thing/>
</certificate>)";
  auto result = internal::ParseRecoveryKeyStoreCertXML(kTestXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->endpoints, testing::ElementsAre("endpoint"));
  EXPECT_THAT(result->intermediates, testing::ElementsAre("intermediate"));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_CertXmlIgnoresNestedCerts) {
  static constexpr std::string_view kTestXml =
      R"(<certificate>
  <intermediates>
    <should-ignore-this>
      <cert>intermediate-should-be-ignored</cert>
    </should-ignore-this>
    <cert>intermediate</cert>
    <also-not-cert/>
  </intermediates>
  <endpoints>
    <should-ignore-this>
      <cert>endpoint-should-be-ignored</cert>
    </should-ignore-this>
    <cert>endpoint</cert>
  </endpoints>
</certificate>)";
  auto result = internal::ParseRecoveryKeyStoreCertXML(kTestXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->endpoints, testing::ElementsAre("endpoint"));
  EXPECT_THAT(result->intermediates, testing::ElementsAre("intermediate"));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_CertXmlNoEndpoints) {
  static constexpr std::string_view kTestXml =
      R"(<certificate>
  <intermediates>
    <cert>intermediate</cert>
  </intermediates>
</certificate>)";
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreCertXML(kTestXml));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_CertXmlNoIntermediates) {
  static constexpr std::string_view kTestXml =
      R"(<certificate>
  <intermediates>
  </intermediates>
  <endpoints>
    <cert>endpoint</cert>
  </endpoints>
</certificate>)";
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreCertXML(kTestXml));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_CertXmlEmptyString) {
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreCertXML(""));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_ParseValidSigXml) {
  auto result = internal::ParseRecoveryKeyStoreSigXML(kSigXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(
      result->intermediates,
      testing::UnorderedElementsAre(GetIntermediate1(), GetIntermediate2()));
  EXPECT_EQ(result->certificate, GetCertificate());
  EXPECT_EQ(result->signature, GetSignature());
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_SigXmlWithWrongRootTag) {
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreSigXML("<not-sig></not-sig>"));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_SigXmlIgnoresEmptyCerts) {
  static constexpr std::string_view kTestXml =
      R"(<signature>
  <intermediates>
    <cert></cert>
    <cert>intermediate</cert>
  </intermediates>
  <certificate>certificate</certificate>
  <value>signature</value>
</signature>)";
  auto result = internal::ParseRecoveryKeyStoreSigXML(kTestXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->intermediates, testing::ElementsAre("intermediate"));
  EXPECT_EQ(result->certificate, "certificate");
  EXPECT_EQ(result->signature, "signature");
}

TEST_F(RecoveryKeyStoreCertificateTest,
       Internals_SigXmlIgnoresUnrecognizedTags) {
  static constexpr std::string_view kTestXml =
      R"(<signature>
  <intermediates>
    <something-else></something-else>
    <cert>intermediate</cert>
    <something-else/>
  </intermediates>
  <unexpected></unexpected>
  <certificate>certificate</certificate>
  <value>signature</value>
  <unexpected/>
</signature>)";
  auto result = internal::ParseRecoveryKeyStoreSigXML(kTestXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->intermediates, testing::ElementsAre("intermediate"));
  EXPECT_EQ(result->certificate, "certificate");
  EXPECT_EQ(result->signature, "signature");
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_SigXmlIgnoresNestedCerts) {
  static constexpr std::string_view kTestXml =
      R"(<signature>
  <intermediates>
    <nested>
      <cert>should ignore this</cert>
    </nested>
    <cert>intermediate</cert>
  </intermediates>
  <unexpected></unexpected>
  <certificate>certificate</certificate>
  <value>signature</value>
  <unexpected/>
</signature>)";
  auto result = internal::ParseRecoveryKeyStoreSigXML(kTestXml);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->intermediates, testing::ElementsAre("intermediate"));
  EXPECT_EQ(result->certificate, "certificate");
  EXPECT_EQ(result->signature, "signature");
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_SigXmlNoIntermediates) {
  static constexpr std::string_view kTestXml =
      R"(<signature>
  <certificate>certificate</certificate>
  <value>signature</value>
</signature>)";
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreSigXML(kTestXml));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_SigXmlNoCertificate) {
  static constexpr std::string_view kTestXml =
      R"(<signature>
  <intermediates>
    <cert>intermediate</cert>
  </intermediates>
  <value>signature</value>
</signature>)";
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreSigXML(kTestXml));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_SigXmlNoSignature) {
  static constexpr std::string_view kTestXml =
      R"(<signature>
  <intermediates>
    <cert>intermediate</cert>
  </intermediates>
  <certificate>certificate</certificate>
</signature>)";
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreSigXML(kTestXml));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_SigXmlEmptyString) {
  EXPECT_FALSE(internal::ParseRecoveryKeyStoreSigXML(""));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_VerifySignatureChainSuccess) {
  std::vector<std::string> intermediates{std::string(GetIntermediate1()),
                                         std::string(GetIntermediate2())};
  std::shared_ptr<const bssl::ParsedCertificate> certificate =
      internal::VerifySignatureChain(GetCertificate(), intermediates,
                                     kValidCertificateDate);
  EXPECT_TRUE(certificate);
}

TEST_F(RecoveryKeyStoreCertificateTest,
       Internals_VerifySignatureChainCertificateNotB64) {
  std::vector<std::string> intermediates{std::string(GetIntermediate1()),
                                         std::string(GetIntermediate2())};
  std::shared_ptr<const bssl::ParsedCertificate> certificate =
      internal::VerifySignatureChain("not base 64", intermediates,
                                     kValidCertificateDate);
  EXPECT_FALSE(certificate);
}

TEST_F(RecoveryKeyStoreCertificateTest,
       Internals_VerifySignatureChainSkipsNotB64Intermediates) {
  std::vector<std::string> intermediates{"not base 64",
                                         std::string(GetIntermediate1()),
                                         std::string(GetIntermediate2())};
  std::shared_ptr<const bssl::ParsedCertificate> certificate =
      internal::VerifySignatureChain(GetCertificate(), intermediates,
                                     kValidCertificateDate);
  EXPECT_TRUE(certificate);
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_VerifySignatureChainExpired) {
  std::vector<std::string> intermediates{std::string(GetIntermediate1()),
                                         std::string(GetIntermediate2())};
  std::shared_ptr<const bssl::ParsedCertificate> certificate =
      internal::VerifySignatureChain(GetCertificate(), intermediates,
                                     kValidCertificateDate + base::Days(10000));
  EXPECT_FALSE(certificate);
}

TEST_F(RecoveryKeyStoreCertificateTest,
       Internals_VerifySignatureChainNotYetValid) {
  std::vector<std::string> intermediates = {std::string(GetIntermediate1()),
                                            std::string(GetIntermediate2())};
  std::shared_ptr<const bssl::ParsedCertificate> certificate =
      internal::VerifySignatureChain(GetCertificate(), intermediates,
                                     kValidCertificateDate - base::Days(10000));
  EXPECT_FALSE(certificate);
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_VerifySignature) {
  std::vector<std::string> intermediates = {std::string(GetIntermediate1()),
                                            std::string(GetIntermediate2())};
  auto certificate = internal::VerifySignatureChain(
      GetCertificate(), intermediates, kValidCertificateDate);
  ASSERT_TRUE(certificate);
  EXPECT_TRUE(internal::VerifySignature(std::move(certificate), kCertXml,
                                        GetSignature()));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_VerifySignatureNotValid) {
  std::vector<std::string> intermediates = {std::string(GetIntermediate1()),
                                            std::string(GetIntermediate2())};
  auto certificate = internal::VerifySignatureChain(
      GetCertificate(), intermediates, kValidCertificateDate);
  ASSERT_TRUE(certificate);
  std::string invalid_signature(GetSignature());
  invalid_signature[0] = 'X';
  EXPECT_FALSE(internal::VerifySignature(std::move(certificate), kCertXml,
                                         invalid_signature));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_VerifySignatureNotBase64) {
  std::vector<std::string> intermediates = {std::string(GetIntermediate1()),
                                            std::string(GetIntermediate2())};
  auto certificate = internal::VerifySignatureChain(
      GetCertificate(), intermediates, kValidCertificateDate);
  ASSERT_TRUE(certificate);
  EXPECT_FALSE(internal::VerifySignature(std::move(certificate), kCertXml,
                                         "not base 64"));
}

TEST_F(RecoveryKeyStoreCertificateTest, Internals_ExtractEndpointPKs) {
  internal::ParsedRecoveryKeyStoreCertXML cert_xml =
      *internal::ParseRecoveryKeyStoreCertXML(kCertXml);
  std::vector<std::unique_ptr<SecureBoxPublicKey>> pks =
      internal::ExtractEndpointPublicKeys(std::move(cert_xml),
                                          kValidCertificateDate);
  ASSERT_EQ(pks.size(), 3u);
  EXPECT_THAT(pks.at(0)->ExportToBytes(),
              testing::ElementsAreArray(kEndpoint1PublicKey));
  EXPECT_THAT(pks.at(1)->ExportToBytes(),
              testing::ElementsAreArray(kEndpoint2PublicKey));
  EXPECT_THAT(pks.at(2)->ExportToBytes(),
              testing::ElementsAreArray(kEndpoint3PublicKey));
}

TEST_F(RecoveryKeyStoreCertificateTest,
       Internals_ExtractEndpointIgnoresBadCerts) {
  internal::ParsedRecoveryKeyStoreCertXML cert_xml =
      *internal::ParseRecoveryKeyStoreCertXML(kCertXml);
  cert_xml.endpoints.emplace_back("aabbcc");
  std::vector<std::unique_ptr<SecureBoxPublicKey>> pks =
      internal::ExtractEndpointPublicKeys(std::move(cert_xml),
                                          kValidCertificateDate);
  EXPECT_EQ(pks.size(), 3u);
}

TEST_F(RecoveryKeyStoreCertificateTest,
       Internals_ExtractEndpointIgnoresBadBase64) {
  internal::ParsedRecoveryKeyStoreCertXML cert_xml =
      *internal::ParseRecoveryKeyStoreCertXML(kCertXml);
  cert_xml.endpoints.emplace_back("not base 64");
  std::vector<std::unique_ptr<SecureBoxPublicKey>> pks =
      internal::ExtractEndpointPublicKeys(std::move(cert_xml),
                                          kValidCertificateDate);
  EXPECT_EQ(pks.size(), 3u);
}

TEST_F(RecoveryKeyStoreCertificateTest, ParseSuccess) {
  std::optional<RecoveryKeyStoreCertificate> recovery_key_store_cert =
      RecoveryKeyStoreCertificate::Parse(kCertXml, kSigXml,
                                         kValidCertificateDate);
  ASSERT_TRUE(recovery_key_store_cert.has_value());
  ASSERT_EQ(recovery_key_store_cert->endpoint_public_keys().size(), 3u);
  EXPECT_THAT(
      recovery_key_store_cert->endpoint_public_keys().at(0)->ExportToBytes(),
      testing::ElementsAreArray(kEndpoint1PublicKey));
  EXPECT_THAT(
      recovery_key_store_cert->endpoint_public_keys().at(1)->ExportToBytes(),
      testing::ElementsAreArray(kEndpoint2PublicKey));
  EXPECT_THAT(
      recovery_key_store_cert->endpoint_public_keys().at(2)->ExportToBytes(),
      testing::ElementsAreArray(kEndpoint3PublicKey));
}

TEST_F(RecoveryKeyStoreCertificateTest, Parse_BadCertXml) {
  EXPECT_FALSE(RecoveryKeyStoreCertificate::Parse("not cert xml", kSigXml,
                                                  kValidCertificateDate));
}

TEST_F(RecoveryKeyStoreCertificateTest, Parse_BadSigXml) {
  EXPECT_FALSE(RecoveryKeyStoreCertificate::Parse(kCertXml, "not sig xml",
                                                  kValidCertificateDate));
}

TEST_F(RecoveryKeyStoreCertificateTest, Parse_BadChain) {
  static constexpr std::string_view kBadSigXml =
      R"(<?xml version="1.0" encoding="UTF-8"?>
<signature>
  <intermediates>
    <cert>intermediate</cert>
  </intermediates>
  <certificate>certificate</certificate>
  <value>signature</value>
</signature>)";
  EXPECT_FALSE(RecoveryKeyStoreCertificate::Parse(kCertXml, kBadSigXml,
                                                  kValidCertificateDate));
}

TEST_F(RecoveryKeyStoreCertificateTest, Parse_BadSignature) {
  std::string sig_xml_bad_signature(kSigXml);
  size_t signature_index = sig_xml_bad_signature.find(GetSignature());
  sig_xml_bad_signature[signature_index] = 'X';
  EXPECT_FALSE(RecoveryKeyStoreCertificate::Parse(
      kCertXml, sig_xml_bad_signature, kValidCertificateDate));
}

}  // namespace

}  // namespace trusted_vault
