// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_auth_util.h"

#include <string>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/media_router/common/providers/cast/certificate/cast_cert_reader.h"
#include "components/media_router/common/providers/cast/certificate/cast_cert_test_helpers.h"
#include "components/media_router/common/providers/cast/certificate/cast_cert_validator.h"
#include "components/media_router/common/providers/cast/certificate/cast_crl.h"
#include "components/media_router/common/providers/cast/channel/cast_auth_util.h"
#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"
#include "net/cert/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"
#include "third_party/openscreen/src/cast/common/certificate/proto/test_suite.pb.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

using openscreen::cast::proto::SHA1;
using openscreen::cast::proto::SHA256;

namespace cast_channel {
namespace {

constexpr CastChannelFlags kFlagsCRLMissing =
    static_cast<CastChannelFlags>(CastChannelFlag::kCRLMissing);
constexpr CastChannelFlags kFlagsSHA1AndCRLMissing =
    static_cast<CastChannelFlags>(CastChannelFlag::kSha1DigestAlgorithm) |
    static_cast<CastChannelFlags>(CastChannelFlag::kCRLMissing);
constexpr CastChannelFlags kFlagsInvalidCRL =
    static_cast<CastChannelFlags>(CastChannelFlag::kCRLInvalid) |
    static_cast<CastChannelFlags>(CastChannelFlag::kCRLMissing);
constexpr CastChannelFlags kFlagsInvalidFallbackCRL =
    static_cast<CastChannelFlags>(CastChannelFlag::kInvalidFallbackCRL) |
    static_cast<CastChannelFlags>(CastChannelFlag::kCRLMissing);
constexpr CastChannelFlags kFlagsAcceptedByFallbackCRL =
    static_cast<CastChannelFlags>(
        CastChannelFlag::kCertificateAcceptedByFallbackCRL) |
    static_cast<CastChannelFlags>(CastChannelFlag::kCRLMissing);
constexpr CastChannelFlags kFlagsExpiredFallbackCRL =
    static_cast<CastChannelFlags>(CastChannelFlag::kInvalidFallbackCRL) |
    static_cast<CastChannelFlags>(CastChannelFlag::kCRLMissing);

class CastAuthUtilTest : public testing::Test {
 public:
  CastAuthUtilTest() = default;
  ~CastAuthUtilTest() override = default;

  void SetUp() override {}

 protected:
  static AuthResponse CreateAuthResponse(
      std::string* signed_data,
      openscreen::cast::proto::HashAlgorithm digest_algorithm) {
    auto chain = cast_certificate::ReadCertificateChainFromFile(
        cast_certificate::testing::GetCastCertificatesSubDirectory()
            .AppendASCII("chromecast_gen1.pem"));
    CHECK(!chain.empty());

    auto signature_data = cast_certificate::testing::ReadSignatureTestData(
        "signeddata/2ZZBG9_FA8FCA3EF91A.pem");

    AuthResponse response;

    response.set_client_auth_certificate(chain[0]);
    for (size_t i = 1; i < chain.size(); ++i)
      response.add_intermediate_certificate(chain[i]);

    response.set_hash_algorithm(digest_algorithm);
    switch (digest_algorithm) {
      case SHA1:
        response.set_signature(signature_data.signature_sha1);
        break;
      case SHA256:
        response.set_signature(signature_data.signature_sha256);
        break;
    }
    *signed_data = signature_data.message;

    return response;
  }

  // Mangles a string by inverting the first byte.
  static void MangleString(std::string* str) { (*str)[0] = ~(*str)[0]; }
};

// Note on expiration: VerifyCredentials() depends on the system clock. In
// practice this shouldn't be a problem though since the certificate chain
// being verified doesn't expire until 2032!
TEST_F(CastAuthUtilTest, VerifySuccess) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  base::Time now = base::Time::Now();
  AuthResult result = VerifyCredentialsForTest(
      auth_response, signed_data, cast_certificate::CRLPolicy::CRL_OPTIONAL,
      nullptr, now);
  EXPECT_TRUE(result.success());
  EXPECT_EQ(static_cast<unsigned>(AuthResult::POLICY_NONE),
            result.channel_policies);
  EXPECT_EQ(kFlagsCRLMissing, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyBadCA) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  MangleString(auth_response.mutable_intermediate_certificate(0));
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_CERT_PARSING_FAILED, result.error_type);
  EXPECT_EQ(kFlagsCRLMissing, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyBadClientAuthCert) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  MangleString(auth_response.mutable_client_auth_certificate());
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result.success());
  // TODO(eroman): Not quite right of an error.
  EXPECT_EQ(AuthResult::ERROR_CERT_PARSING_FAILED, result.error_type);
  EXPECT_EQ(kFlagsCRLMissing, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyBadSignature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kEnforceFallbackCRLRevocationChecking);
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  MangleString(auth_response.mutable_signature());
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_SIGNED_BLOBS_MISMATCH, result.error_type);
  EXPECT_EQ(kFlagsCRLMissing, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyEmptySignature) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kEnforceFallbackCRLRevocationChecking);
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  auth_response.mutable_signature()->clear();
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_SIGNATURE_EMPTY, result.error_type);
  EXPECT_EQ(kFlagsCRLMissing, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyUnsupportedDigest) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnforceSHA256Checking);
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA1);
  base::Time now = base::Time::Now();
  AuthResult result = VerifyCredentialsForTest(
      auth_response, signed_data, cast_certificate::CRLPolicy::CRL_OPTIONAL,
      nullptr, now);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_DIGEST_UNSUPPORTED, result.error_type);
  EXPECT_EQ(kFlagsSHA1AndCRLMissing, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyBackwardsCompatibleDigest) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA1);
  base::Time now = base::Time::Now();
  AuthResult result = VerifyCredentialsForTest(
      auth_response, signed_data, cast_certificate::CRLPolicy::CRL_OPTIONAL,
      nullptr, now);
  EXPECT_TRUE(result.success());
  EXPECT_EQ(kFlagsSHA1AndCRLMissing, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyCrlRequiredWithFallback) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  base::Time now = base::Time::Now();
  AuthResult result = VerifyCredentialsForTest(
      auth_response, signed_data,
      cast_certificate::CRLPolicy::CRL_REQUIRED_WITH_FALLBACK, nullptr, now);
  EXPECT_TRUE(result.success());
  EXPECT_EQ(static_cast<unsigned>(AuthResult::POLICY_NONE),
            result.channel_policies);
  EXPECT_EQ(kFlagsAcceptedByFallbackCRL, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyCrlOptionalWithFallback) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  base::Time now = base::Time::Now();
  AuthResult result = VerifyCredentialsForTest(
      auth_response, signed_data,
      cast_certificate::CRLPolicy::CRL_OPTIONAL_WITH_FALLBACK, nullptr, now);
  EXPECT_TRUE(result.success());
  EXPECT_EQ(static_cast<unsigned>(AuthResult::POLICY_NONE),
            result.channel_policies);
  EXPECT_EQ(kFlagsAcceptedByFallbackCRL, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyCrlRequiredWithExpiredFallback) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  base::Time now = base::Time::Now() + base::Seconds(12096000);  // 20 weeks
  AuthResult result = VerifyCredentialsForTest(
      auth_response, signed_data,
      cast_certificate::CRLPolicy::CRL_REQUIRED_WITH_FALLBACK, nullptr, now);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_FALLBACK_CRL_INVALID, result.error_type);
  EXPECT_EQ(kFlagsExpiredFallbackCRL, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyCrlRequiredWithNotExpiredFallback) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  base::Time now = base::Time::Now() + base::Seconds(10);
  AuthResult result = VerifyCredentialsForTest(
      auth_response, signed_data,
      cast_certificate::CRLPolicy::CRL_REQUIRED_WITH_FALLBACK, nullptr, now);
  EXPECT_TRUE(result.success());
  EXPECT_EQ(static_cast<unsigned>(AuthResult::POLICY_NONE),
            result.channel_policies);
  EXPECT_EQ(kFlagsAcceptedByFallbackCRL, result.flags);
}

TEST_F(CastAuthUtilTest, FeatureFlagVerifyCrlRequiredWithFallbackCRL) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      kEnforceFallbackCRLRevocationChecking);
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_TRUE(result.success());
  EXPECT_EQ(static_cast<unsigned>(AuthResult::POLICY_NONE),
            result.channel_policies);
  EXPECT_EQ(kFlagsAcceptedByFallbackCRL, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyCrlRequiredWithInvalidFallbackCRL) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  base::Time now = base::Time::Now() + base::Hours(100000000);
  AuthResult result = VerifyCredentialsForTest(
      auth_response, signed_data,
      cast_certificate::CRLPolicy::CRL_REQUIRED_WITH_FALLBACK, nullptr, now);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA, result.error_type);
  EXPECT_EQ(kFlagsInvalidFallbackCRL, result.flags);
}

TEST_F(CastAuthUtilTest,
       VerifyInvalidCRLWithFeatureFlagEnforceRevocationChecking) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kEnforceRevocationChecking},
                                       {kEnforceFallbackCRLRevocationChecking});
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_CRL_INVALID, result.error_type);
  EXPECT_EQ(kFlagsInvalidCRL, result.flags);
}

TEST_F(CastAuthUtilTest,
       VerifyMissingCRLWithoutFeatureFlagEnforceRevocationChecking) {
  base::test::ScopedFeatureList scoped_feature_list;
  std::string signed_data;
  scoped_feature_list.InitWithFeatures(
      {}, {kEnforceRevocationChecking, kEnforceFallbackCRLRevocationChecking});
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_TRUE(result.success());
  EXPECT_EQ(AuthResult::ERROR_NONE, result.error_type);
  EXPECT_EQ(kFlagsCRLMissing, result.flags);
}

TEST_F(CastAuthUtilTest,
       VerifyParsingErrorWithFeatureFlagEnforceRevocationChecking) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({kEnforceRevocationChecking},
                                       {kEnforceFallbackCRLRevocationChecking});

  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  MangleString(auth_response.mutable_client_auth_certificate());
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result.success());
  // will early return if the CRL is invalid
  EXPECT_EQ(AuthResult::ERROR_CRL_INVALID, result.error_type);
  EXPECT_EQ(kFlagsInvalidCRL, result.flags);
}

TEST_F(CastAuthUtilTest,
       VerifyParsingErrorWithoutFeatureFlagEnforceRevocationChecking) {
  base::test::ScopedFeatureList scoped_feature_list;

  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  MangleString(auth_response.mutable_client_auth_certificate());
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result.success());
  // will NOT early return if the flag is off
  EXPECT_EQ(AuthResult::ERROR_CERT_PARSING_FAILED, result.error_type);
  EXPECT_EQ(kFlagsCRLMissing, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyCrlOptionalWithInvalidFallbackCRL) {
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  base::Time now = base::Time::Now() + base::Hours(100000000);
  AuthResult result = VerifyCredentialsForTest(
      auth_response, signed_data,
      cast_certificate::CRLPolicy::CRL_OPTIONAL_WITH_FALLBACK, nullptr, now);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA, result.error_type);
  EXPECT_EQ(kFlagsInvalidFallbackCRL, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyBadPeerCert) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      kEnforceFallbackCRLRevocationChecking);
  std::string signed_data;
  AuthResponse auth_response = CreateAuthResponse(&signed_data, SHA256);
  MangleString(&signed_data);
  AuthResult result = VerifyCredentials(auth_response, signed_data);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_SIGNED_BLOBS_MISMATCH, result.error_type);
  EXPECT_EQ(kFlagsCRLMissing, result.flags);
}

TEST_F(CastAuthUtilTest, VerifySenderNonceMatch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnforceNonceChecking);
  AuthContext context = AuthContext::Create();
  AuthResult result = context.VerifySenderNonce(context.nonce());
  EXPECT_TRUE(result.success());
  EXPECT_EQ(kCastChannelFlagsNone, result.flags);
}

TEST_F(CastAuthUtilTest, VerifySenderNonceMismatch) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnforceNonceChecking);
  AuthContext context = AuthContext::Create();
  std::string received_nonce = "test2";
  EXPECT_NE(received_nonce, context.nonce());
  AuthResult result = context.VerifySenderNonce(received_nonce);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_SENDER_NONCE_MISMATCH, result.error_type);
  EXPECT_EQ(
      static_cast<CastChannelFlags>(CastChannelFlag::kSenderNonceMismatch),
      result.flags);
}

TEST_F(CastAuthUtilTest, VerifySenderNonceMissing) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnforceNonceChecking);
  AuthContext context = AuthContext::Create();
  std::string received_nonce;
  EXPECT_FALSE(context.nonce().empty());
  AuthResult result = context.VerifySenderNonce(received_nonce);
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_SENDER_NONCE_MISMATCH, result.error_type);
  EXPECT_EQ(static_cast<CastChannelFlags>(CastChannelFlag::kSenderNonceMissing),
            result.flags);
}

TEST_F(CastAuthUtilTest, VerifyTLSCertificateSuccess) {
  auto tls_cert_der = cast_certificate::ReadCertificateChainFromFile(
      cast_certificate::testing::GetCastCertificatesSubDirectory().AppendASCII(
          "test_tls_cert.pem"));

  scoped_refptr<net::X509Certificate> tls_cert =
      net::X509Certificate::CreateFromBytes(
          base::as_bytes(base::make_span(tls_cert_der[0])));
  std::string peer_cert_der;
  AuthResult result =
      VerifyTLSCertificate(*tls_cert, &peer_cert_der, tls_cert->valid_start());
  EXPECT_TRUE(result.success());
  EXPECT_EQ(kCastChannelFlagsNone, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyTLSCertificateTooEarly) {
  auto tls_cert_der = cast_certificate::ReadCertificateChainFromFile(
      cast_certificate::testing::GetCastCertificatesSubDirectory().AppendASCII(
          "test_tls_cert.pem"));

  scoped_refptr<net::X509Certificate> tls_cert =
      net::X509Certificate::CreateFromBytes(
          base::as_bytes(base::make_span(tls_cert_der[0])));
  std::string peer_cert_der;
  AuthResult result = VerifyTLSCertificate(
      *tls_cert, &peer_cert_der, tls_cert->valid_start() - base::Seconds(1));
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_TLS_CERT_VALID_START_DATE_IN_FUTURE,
            result.error_type);
  EXPECT_EQ(kCastChannelFlagsNone, result.flags);
}

TEST_F(CastAuthUtilTest, VerifyTLSCertificateTooLate) {
  auto tls_cert_der = cast_certificate::ReadCertificateChainFromFile(
      cast_certificate::testing::GetCastCertificatesSubDirectory().AppendASCII(
          "test_tls_cert.pem"));

  scoped_refptr<net::X509Certificate> tls_cert =
      net::X509Certificate::CreateFromBytes(
          base::as_bytes(base::make_span(tls_cert_der[0])));
  std::string peer_cert_der;
  AuthResult result = VerifyTLSCertificate(
      *tls_cert, &peer_cert_der, tls_cert->valid_expiry() + base::Seconds(2));
  EXPECT_FALSE(result.success());
  EXPECT_EQ(AuthResult::ERROR_TLS_CERT_EXPIRED, result.error_type);
  EXPECT_EQ(kCastChannelFlagsNone, result.flags);
}

// Indicates the expected result of test step's verification.
enum TestStepResult {
  RESULT_SUCCESS,
  RESULT_FAIL,
};

// Verifies that the certificate chain provided is not revoked according to
// the provided Cast CRL at |verification_time|.
// The provided CRL is verified at |verification_time|.
// If |crl_required| is set, then a valid Cast CRL must be provided.
// Otherwise, a missing CRL is be ignored.
AuthResult TestVerifyRevocation(
    const std::vector<std::string>& certificate_chain,
    const std::string& crl_bundle,
    const base::Time& verification_time,
    bool crl_required,
    bssl::TrustStore* crl_trust_store) {
  AuthResponse response;

  if (certificate_chain.size() > 0) {
    response.set_client_auth_certificate(certificate_chain[0]);
    for (size_t i = 1; i < certificate_chain.size(); ++i)
      response.add_intermediate_certificate(certificate_chain[i]);
  }

  response.set_crl(crl_bundle);

  cast_certificate::CRLPolicy crl_policy =
      cast_certificate::CRLPolicy::CRL_REQUIRED;
  if (!crl_required && crl_bundle.empty())
    crl_policy = cast_certificate::CRLPolicy::CRL_OPTIONAL;
  AuthResult result = VerifyCredentialsForTest(
      response, "", crl_policy, crl_trust_store, verification_time);
  // This test doesn't set the signature so it will just fail there.
  EXPECT_FALSE(result.success());
  return result;
}

// Runs a single test case.
bool RunTest(const openscreen::cast::proto::DeviceCertTest& test_case) {
  std::unique_ptr<cast_certificate::testing::ScopedCastTrustStoreConfig>
      scoped_cast_trust_store =
          test_case.use_test_trust_anchors()
              ? cast_certificate::testing::ScopedCastTrustStoreConfig::
                    TestCertificates("cast_test_root_ca.pem")
              : cast_certificate::testing::ScopedCastTrustStoreConfig::
                    BuiltInCertificates();
  CHECK(scoped_cast_trust_store)
      << "Failed to create Cast trust store configuration";
  std::unique_ptr<bssl::TrustStoreInMemory> crl_trust_store =
      test_case.use_test_trust_anchors()
          ? cast_certificate::testing::LoadTestCert("cast_crl_test_root_ca.pem")
          : nullptr;

  std::vector<std::string> certificate_chain;
  for (auto const& cert : test_case.der_cert_path()) {
    certificate_chain.push_back(cert);
  }

  // CastAuthUtil verifies the CRL at the same time as the certificate.
  base::Time verification_time;
  uint64_t cert_verify_time = test_case.cert_verification_time_seconds();
  if (cert_verify_time) {
    verification_time = cast_certificate::testing::ConvertUnixTimestampSeconds(
        cert_verify_time);
  } else {
    verification_time = cast_certificate::testing::ConvertUnixTimestampSeconds(
        test_case.crl_verification_time_seconds());
  }

  std::string crl_bundle = test_case.crl_bundle();
  AuthResult result;
  switch (test_case.expected_result()) {
    case openscreen::cast::proto::PATH_VERIFICATION_FAILED:
      result =
          TestVerifyRevocation(certificate_chain, crl_bundle, verification_time,
                               false, crl_trust_store.get());
      EXPECT_EQ(result.error_type,
                AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA);
      return result.error_type ==
             AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA;
    case openscreen::cast::proto::CRL_VERIFICATION_FAILED:
    // Fall-through intended.
    case openscreen::cast::proto::REVOCATION_CHECK_FAILED_WITHOUT_CRL:
      result =
          TestVerifyRevocation(certificate_chain, crl_bundle, verification_time,
                               true, crl_trust_store.get());
      EXPECT_EQ(result.error_type, AuthResult::ERROR_CRL_INVALID);
      return result.error_type == AuthResult::ERROR_CRL_INVALID;
    case openscreen::cast::proto::CRL_EXPIRED_AFTER_INITIAL_VERIFICATION:
      // By-pass this test because CRL is always verified at the time the
      // certificate is verified.
      return true;
    case openscreen::cast::proto::REVOCATION_CHECK_FAILED:
      result =
          TestVerifyRevocation(certificate_chain, crl_bundle, verification_time,
                               true, crl_trust_store.get());
      EXPECT_EQ(result.error_type, AuthResult::ERROR_CERT_REVOKED);
      return result.error_type == AuthResult::ERROR_CERT_REVOKED;
    case openscreen::cast::proto::SUCCESS:
      result =
          TestVerifyRevocation(certificate_chain, crl_bundle, verification_time,
                               false, crl_trust_store.get());
      EXPECT_EQ(result.error_type, AuthResult::ERROR_SIGNED_BLOBS_MISMATCH);
      return result.error_type == AuthResult::ERROR_SIGNED_BLOBS_MISMATCH;
    case openscreen::cast::proto::UNKNOWN:
      return false;
  }
  return false;
}

// Parses the provided test suite provided in wire-format proto.
// Each test contains the inputs and the expected output.
// To see the description of the test, execute the test.
// These tests are generated by a test generator in google3.
void RunTestSuite(const std::string& test_suite_file_name) {
  std::string testsuite_raw;
  base::ReadFileToString(
      cast_certificate::testing::GetCastCertificateDirectory().AppendASCII(
          test_suite_file_name),
      &testsuite_raw);

  openscreen::cast::proto::DeviceCertTestSuite test_suite;
  EXPECT_TRUE(test_suite.ParseFromString(testsuite_raw));
  uint16_t success = 0;
  uint16_t failed = 0;
  std::vector<std::string> failed_tests;

  for (auto const& test_case : test_suite.tests()) {
    LOG(INFO) << "[ RUN      ] " << test_case.description();
    bool result = RunTest(test_case);
    EXPECT_TRUE(result);
    if (!result) {
      LOG(INFO) << "[  FAILED  ] " << test_case.description();
      ++failed;
      failed_tests.push_back(test_case.description());
    } else {
      LOG(INFO) << "[  PASSED  ] " << test_case.description();
      ++success;
    }
  }
  LOG(INFO) << "[  PASSED  ] " << success << " test(s).";
  if (failed) {
    LOG(INFO) << "[  FAILED  ] " << failed << " test(s), listed below:";
    for (const auto& failed_test : failed_tests) {
      LOG(INFO) << "[  FAILED  ] " << failed_test;
    }
  }
}

TEST_F(CastAuthUtilTest, CRLTestSuite) {
  RunTestSuite("testsuite/testsuite1.pb");
}

}  // namespace
}  // namespace cast_channel
