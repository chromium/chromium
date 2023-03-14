// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/onc/onc_certificate_pattern.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {
constexpr char kFakePemEncodedIssuer[] = "PEM-ENCODED-ISSUER";

class OncCertificatePatternTest : public testing::Test {
 public:
  OncCertificatePatternTest() = default;

  OncCertificatePatternTest(const OncCertificatePatternTest&) = delete;
  OncCertificatePatternTest& operator=(const OncCertificatePatternTest&) =
      delete;

  ~OncCertificatePatternTest() override = default;

  void SetUp() override {
    cert_ =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "client_1.pem");
    ASSERT_TRUE(cert_);
  }

  void TearDown() override { cert_.reset(); }

 protected:
  scoped_refptr<net::X509Certificate> cert_;
};

}  // namespace

TEST_F(OncCertificatePatternTest, EmptyPattern) {
  OncCertificatePattern pattern;
  EXPECT_TRUE(pattern.Empty());

  EXPECT_TRUE(pattern.Matches(*cert_, kFakePemEncodedIssuer));
}

TEST_F(OncCertificatePatternTest, ParsePatternFromOnc) {
  const char* pattern_json = R"(
    {
      "Issuer": {
        "CommonName": "Issuer CN",
        "Locality": "Issuer L",
        "Organization": "Issuer O",
        "OrganizationalUnit": "Issuer OU",
      },
      "Subject": {
        "CommonName": "Subject CN",
        "Locality": "Subject L",
        "Organization": "Subject O",
        "OrganizationalUnit": "Subject OU",
      },
      "IssuerCAPEMs": [ "PEM1", "PEM2" ]
    })";
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      pattern_json, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;

  auto pattern =
      OncCertificatePattern::ReadFromONCDictionary(parsed_json->GetDict());
  ASSERT_TRUE(pattern);
  EXPECT_FALSE(pattern.value().Empty());

  EXPECT_EQ("Issuer CN", pattern.value().issuer_pattern().common_name());
  EXPECT_EQ("Issuer L", pattern.value().issuer_pattern().locality());
  EXPECT_EQ("Issuer O", pattern.value().issuer_pattern().organization());
  EXPECT_EQ("Issuer OU", pattern.value().issuer_pattern().organization_unit());
  EXPECT_EQ("Subject CN", pattern.value().subject_pattern().common_name());
  EXPECT_EQ("Subject L", pattern.value().subject_pattern().locality());
  EXPECT_EQ("Subject O", pattern.value().subject_pattern().organization());
  EXPECT_EQ("Subject OU",
            pattern.value().subject_pattern().organization_unit());
  EXPECT_THAT(pattern.value().pem_encoded_issuer_cas(),
              testing::ElementsAre("PEM1", "PEM2"));
}

TEST_F(OncCertificatePatternTest, PatternMatchingIssuer) {
  // Note: We're only testing matching of "CommonName", assuming that matching
  // of the other principal fields is verified by the tests for
  // |certificate_matching::CertificatePrincipalPattern|.
  const char* pattern_json = R"(
    {
      "Issuer": {
        "CommonName": "B CA"
      }
    })";
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      pattern_json, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;

  {
    auto pattern =
        OncCertificatePattern::ReadFromONCDictionary(parsed_json->GetDict());
    ASSERT_TRUE(pattern);
    EXPECT_FALSE(pattern.value().Empty());

    EXPECT_TRUE(pattern.value().Matches(*cert_, kFakePemEncodedIssuer));
  }

  {
    base::Value::Dict* issuer = parsed_json->GetDict().FindDict("Issuer");
    ASSERT_TRUE(issuer);
    issuer->Set("CommonName", "SomeOtherCA");

    auto pattern =
        OncCertificatePattern::ReadFromONCDictionary(parsed_json->GetDict());
    ASSERT_TRUE(pattern);
    EXPECT_FALSE(pattern.value().Matches(*cert_, kFakePemEncodedIssuer));
  }
}

TEST_F(OncCertificatePatternTest, PatternMatchingSubject) {
  // Note: We're only testing matching of "CommonName", assuming that matching
  // of the other principal fields is verified by the tests for
  // |certificate_matching::CertificatePrincipalPattern|.
  const char* pattern_json = R"(
    {
      "Subject": {
        "CommonName": "Client Cert A"
      }
    })";
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      pattern_json, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;

  {
    auto pattern =
        OncCertificatePattern::ReadFromONCDictionary(parsed_json->GetDict());
    ASSERT_TRUE(pattern);
    EXPECT_FALSE(pattern.value().Empty());

    EXPECT_TRUE(pattern.value().Matches(*cert_, kFakePemEncodedIssuer));
  }

  {
    base::Value::Dict* issuer = parsed_json->GetDict().FindDict("Subject");
    ASSERT_TRUE(issuer);
    issuer->Set("CommonName", "B CA");

    auto pattern =
        OncCertificatePattern::ReadFromONCDictionary(parsed_json->GetDict());
    ASSERT_TRUE(pattern);
    EXPECT_FALSE(pattern.value().Matches(*cert_, kFakePemEncodedIssuer));
  }
}

TEST_F(OncCertificatePatternTest, PatternMatchingIssuerCAPEM) {
  constexpr char kPatternJson[] = R"(
    {
      "IssuerCAPEMs": ["PEM-ENCODED-ISSUER"]
    })";
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      kPatternJson, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;

  auto pattern =
      OncCertificatePattern::ReadFromONCDictionary(parsed_json->GetDict());
  ASSERT_TRUE(pattern);
  EXPECT_FALSE(pattern.value().Empty());

  EXPECT_TRUE(pattern.value().Matches(*cert_, kFakePemEncodedIssuer));
  EXPECT_FALSE(pattern.value().Matches(*cert_, "OtherIssuer"));
}

}  // namespace ash
