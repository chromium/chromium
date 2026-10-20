// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/context_delegate.h"

#include <string>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/enterprise/client_certificates/core/features.h"
#include "components/enterprise/client_certificates/core/metrics_util.h"
#include "net/cert/x509_certificate.h"
#include "net/test/cert_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace client_certificates {

namespace {

constexpr char kTestLoggingContext[] = "TestContext";
constexpr char kTestExpectedCommonName[] = "expected-common-name";
constexpr char kSanitizationHistogram[] =
    "Enterprise.ClientCertificate.CommonNameSanitization";

scoped_refptr<net::X509Certificate> CreateCert(const std::string& common_name,
                                               base::Time not_before,
                                               base::Time not_after) {
  auto [leaf, root] = net::CertBuilder::CreateSimpleChain2();
  leaf->SetSubjectCommonName(common_name);
  leaf->SetValidity(not_before, not_after);
  return leaf->GetX509Certificate();
}

class FakeContextDelegate : public ContextDelegate {
 public:
  explicit FakeContextDelegate(
      std::string expected_common_name = kTestExpectedCommonName)
      : expected_common_name_(std::move(expected_common_name)) {}

  void OnClientCertificateDeleted(
      scoped_refptr<net::X509Certificate> certificate) override {}
  std::string GetExpectedCommonName() const override {
    return expected_common_name_;
  }
  std::string GetIdentityName() override { return "identity"; }
  std::string GetTemporaryIdentityName() override { return "temp_identity"; }
  std::string GetPolicyPref() override { return "policy_pref"; }
  std::string GetLoggingContext() override { return kTestLoggingContext; }

 private:
  std::string expected_common_name_;
};

}  // namespace

TEST(SanitizeAndTruncateCommonNameTest, UnchangedAsciiAndVisiblePunctuation) {
  {
    base::HistogramTester histogram_tester;
    EXPECT_EQ(SanitizeAndTruncateCommonName("my-hostname"), "my-hostname");
    histogram_tester.ExpectUniqueSample(kSanitizationHistogram,
                                        CommonNameSanitizationType::kNone, 1);
  }
  // Visible Unicode punctuation (curly apostrophe U+2019, en-dash U+2013,
  // em-dash U+2014, bullet U+2022, ellipsis U+2026) is preserved.
  {
    base::HistogramTester histogram_tester;
    constexpr char kMacStyleHostname[] =
        "John\xE2\x80\x99s MacBook Pro \xE2\x80\x93 \xE2\x80\x94 "
        "\xE2\x80\xA2 \xE2\x80\xA6";
    EXPECT_EQ(SanitizeAndTruncateCommonName(kMacStyleHostname),
              kMacStyleHostname);
    histogram_tester.ExpectUniqueSample(kSanitizationHistogram,
                                        CommonNameSanitizationType::kNone, 1);
  }
}

TEST(SanitizeAndTruncateCommonNameTest, TrimsLeadingAndTrailingWhitespace) {
  {
    base::HistogramTester histogram_tester;
    EXPECT_EQ(SanitizeAndTruncateCommonName("  my-hostname  "), "my-hostname");
    histogram_tester.ExpectUniqueSample(
        kSanitizationHistogram, CommonNameSanitizationType::kWhitespaceTrimmed,
        1);
  }
  // Leading/trailing Unicode whitespace stripped (U+3000 ideographic space,
  // U+00A0 NBSP).
  {
    base::HistogramTester histogram_tester;
    EXPECT_EQ(SanitizeAndTruncateCommonName("\xE3\x80\x80my-host\xC2\xA0"),
              "my-host");
    histogram_tester.ExpectUniqueSample(
        kSanitizationHistogram, CommonNameSanitizationType::kWhitespaceTrimmed,
        1);
  }
}

TEST(SanitizeAndTruncateCommonNameTest,
     StripsControlFormatAndInvalidUtf8Characters) {
  // C0 control characters and DEL.
  {
    base::HistogramTester histogram_tester;
    EXPECT_EQ(SanitizeAndTruncateCommonName("host\nname\r\t\x7f"), "hostname");
    histogram_tester.ExpectUniqueSample(
        kSanitizationHistogram,
        CommonNameSanitizationType::kControlCharactersStripped, 1);
  }
  // Bidi formatting characters (U+202E), line separator (U+2028), zero-width
  // space (U+200B), and BOM (U+FEFF) removed.
  {
    base::HistogramTester histogram_tester;
    EXPECT_EQ(SanitizeAndTruncateCommonName(
                  "\xEF\xBB\xBFhost\xE2\x80\x8B\xE2\x80\xAEname\xE2\x80\xA8"),
              "hostname");
    histogram_tester.ExpectUniqueSample(
        kSanitizationHistogram,
        CommonNameSanitizationType::kControlCharactersStripped, 1);
  }
  // C1 control character (U+0085 NEL) stripped.
  {
    base::HistogramTester histogram_tester;
    EXPECT_EQ(SanitizeAndTruncateCommonName("host\xC2\x85name"), "hostname");
    histogram_tester.ExpectUniqueSample(
        kSanitizationHistogram,
        CommonNameSanitizationType::kControlCharactersStripped, 1);
  }
  // Invalid UTF-8 bytes are skipped safely and logged.
  {
    base::HistogramTester histogram_tester;
    EXPECT_EQ(SanitizeAndTruncateCommonName("host\xFF\xC0name"), "hostname");
    histogram_tester.ExpectUniqueSample(
        kSanitizationHistogram,
        CommonNameSanitizationType::kControlCharactersStripped, 1);
  }
}

TEST(SanitizeAndTruncateCommonNameTest, EmptyResultAfterSanitization) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(SanitizeAndTruncateCommonName("  \t\r\n\x7f\xE2\x80\xAE  "), "");
  histogram_tester.ExpectBucketCount(
      kSanitizationHistogram,
      CommonNameSanitizationType::kControlCharactersStripped, 1);
  histogram_tester.ExpectBucketCount(
      kSanitizationHistogram, CommonNameSanitizationType::kWhitespaceTrimmed,
      1);
  histogram_tester.ExpectBucketCount(
      kSanitizationHistogram, CommonNameSanitizationType::kEmptyResult, 1);
  histogram_tester.ExpectTotalCount(kSanitizationHistogram, 3);
}

TEST(SanitizeAndTruncateCommonNameTest, TruncatesAtUtf8ByteBoundary) {
  // Exact 64-byte string is not truncated.
  {
    base::HistogramTester histogram_tester;
    std::string exact_64(64, 'a');
    EXPECT_EQ(SanitizeAndTruncateCommonName(exact_64), exact_64);
    histogram_tester.ExpectUniqueSample(kSanitizationHistogram,
                                        CommonNameSanitizationType::kNone, 1);
  }

  // Truncation to 64 UTF-8 bytes.
  {
    base::HistogramTester histogram_tester;
    std::string long_ascii(70, 'a');
    EXPECT_EQ(SanitizeAndTruncateCommonName(long_ascii), std::string(64, 'a'));
    histogram_tester.ExpectUniqueSample(
        kSanitizationHistogram, CommonNameSanitizationType::kTruncated, 1);
  }

  // Multi-byte UTF-8 boundary: 63 'a's followed by a 2-byte character (U+00E9,
  // 0xC3 0xA9) should truncate before the 2-byte character to 63 bytes.
  {
    base::HistogramTester histogram_tester;
    std::string utf8_boundary = std::string(63, 'a') + "\xC3\xA9";
    EXPECT_EQ(SanitizeAndTruncateCommonName(utf8_boundary),
              std::string(63, 'a'));
    histogram_tester.ExpectUniqueSample(
        kSanitizationHistogram, CommonNameSanitizationType::kTruncated, 1);
  }

  // Truncation ending in whitespace: trailing whitespace exposed by truncation
  // (including before a split multi-byte UTF-8 character) is stripped and logs
  // both kTruncated and kWhitespaceTrimmed.
  {
    base::HistogramTester histogram_tester;
    std::string trailing_ws = std::string(62, 'a') + " \xC3\xA9";
    EXPECT_EQ(SanitizeAndTruncateCommonName(trailing_ws), std::string(62, 'a'));
    histogram_tester.ExpectBucketCount(
        kSanitizationHistogram, CommonNameSanitizationType::kTruncated, 1);
    histogram_tester.ExpectBucketCount(
        kSanitizationHistogram, CommonNameSanitizationType::kWhitespaceTrimmed,
        1);
    histogram_tester.ExpectTotalCount(kSanitizationHistogram, 2);
  }
}

class ContextDelegateCertificateValidationTest : public testing::Test {
 protected:
  ContextDelegateCertificateValidationTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kClientCertCommonNameAttribute);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ContextDelegateCertificateValidationTest, MatchingIdentity) {
  base::HistogramTester histogram_tester;
  FakeContextDelegate delegate;
  EXPECT_TRUE(delegate.GetConfigPolicyPref().empty());
  auto cert =
      CreateCert(kTestExpectedCommonName, base::Time::Now() - base::Days(1),
                 base::Time::Now() + base::Days(30));
  EXPECT_FALSE(delegate.ShouldCertificateBeRenewed(
      *cert, delegate.GetExpectedCommonName()));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ClientCertificate.TestContext.CertificateValidation.Result",
      CertificateValidationResult::kValid, 1);
}

TEST_F(ContextDelegateCertificateValidationTest, MismatchedIdentity) {
  base::HistogramTester histogram_tester;
  FakeContextDelegate delegate;
  auto cert = CreateCert("wrong-common-name", base::Time::Now() - base::Days(1),
                         base::Time::Now() + base::Days(30));
  EXPECT_TRUE(delegate.ShouldCertificateBeRenewed(
      *cert, delegate.GetExpectedCommonName()));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ClientCertificate.TestContext.CertificateValidation.Result",
      CertificateValidationResult::kIdentityMismatch, 1);
}

TEST_F(ContextDelegateCertificateValidationTest, EmptySubjectCommonName) {
  base::HistogramTester histogram_tester;
  FakeContextDelegate delegate;
  auto cert = CreateCert("", base::Time::Now() - base::Days(1),
                         base::Time::Now() + base::Days(30));
  EXPECT_TRUE(delegate.ShouldCertificateBeRenewed(
      *cert, delegate.GetExpectedCommonName()));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ClientCertificate.TestContext.CertificateValidation.Result",
      CertificateValidationResult::kEmptySubjectCommonName, 1);
}

TEST_F(ContextDelegateCertificateValidationTest,
       MissingContextIdentity_DoesNotTriggerRenewal) {
  base::HistogramTester histogram_tester;
  FakeContextDelegate delegate(/*expected_common_name=*/"");
  auto cert =
      CreateCert(kTestExpectedCommonName, base::Time::Now() - base::Days(1),
                 base::Time::Now() + base::Days(30));
  EXPECT_FALSE(delegate.ShouldCertificateBeRenewed(
      *cert, delegate.GetExpectedCommonName()));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ClientCertificate.TestContext.CertificateValidation.Result",
      CertificateValidationResult::kMissingContextIdentity, 1);
}

TEST_F(ContextDelegateCertificateValidationTest, ExpiringSoon) {
  base::HistogramTester histogram_tester;
  FakeContextDelegate delegate;
  auto cert =
      CreateCert(kTestExpectedCommonName, base::Time::Now() - base::Days(1),
                 base::Time::Now() + base::Days(5));
  EXPECT_TRUE(delegate.ShouldCertificateBeRenewed(
      *cert, delegate.GetExpectedCommonName()));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ClientCertificate.TestContext.CertificateValidation.Result",
      CertificateValidationResult::kExpiringSoon, 1);
}

TEST_F(ContextDelegateCertificateValidationTest, Expired) {
  base::HistogramTester histogram_tester;
  FakeContextDelegate delegate;
  auto cert =
      CreateCert(kTestExpectedCommonName, base::Time::Now() - base::Days(30),
                 base::Time::Now() - base::Days(1));
  EXPECT_TRUE(delegate.ShouldCertificateBeRenewed(
      *cert, delegate.GetExpectedCommonName()));
  histogram_tester.ExpectUniqueSample(
      "Enterprise.ClientCertificate.TestContext.CertificateValidation.Result",
      CertificateValidationResult::kExpired, 1);
}

TEST_F(ContextDelegateCertificateValidationTest, FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kClientCertCommonNameAttribute);

  {
    base::HistogramTester histogram_tester;
    FakeContextDelegate delegate;
    auto cert =
        CreateCert("wrong-common-name", base::Time::Now() - base::Days(1),
                   base::Time::Now() + base::Days(30));
    EXPECT_FALSE(delegate.ShouldCertificateBeRenewed(
        *cert, delegate.GetExpectedCommonName()));
    histogram_tester.ExpectUniqueSample(
        "Enterprise.ClientCertificate.TestContext.CertificateValidation.Result",
        CertificateValidationResult::kIdentityMismatch, 1);
  }

  {
    base::HistogramTester histogram_tester;
    FakeContextDelegate delegate;
    auto cert = CreateCert("", base::Time::Now() - base::Days(1),
                           base::Time::Now() + base::Days(30));
    EXPECT_FALSE(delegate.ShouldCertificateBeRenewed(
        *cert, delegate.GetExpectedCommonName()));
    histogram_tester.ExpectUniqueSample(
        "Enterprise.ClientCertificate.TestContext.CertificateValidation.Result",
        CertificateValidationResult::kEmptySubjectCommonName, 1);
  }

  {
    base::HistogramTester histogram_tester;
    FakeContextDelegate delegate(/*expected_common_name=*/"");
    auto cert =
        CreateCert(kTestExpectedCommonName, base::Time::Now() - base::Days(1),
                   base::Time::Now() + base::Days(30));
    EXPECT_FALSE(delegate.ShouldCertificateBeRenewed(
        *cert, delegate.GetExpectedCommonName()));
    histogram_tester.ExpectUniqueSample(
        "Enterprise.ClientCertificate.TestContext.CertificateValidation.Result",
        CertificateValidationResult::kMissingContextIdentity, 1);
  }
}

}  // namespace client_certificates
