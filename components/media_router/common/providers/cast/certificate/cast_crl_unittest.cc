// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/certificate/cast_crl.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/media_router/common/providers/cast/certificate/cast_cert_reader.h"
#include "components/media_router/common/providers/cast/certificate/cast_cert_test_helpers.h"
#include "components/media_router/common/providers/cast/certificate/cast_cert_validator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/trust_store_in_memory.h"
#include "third_party/openscreen/src/cast/common/certificate/proto/test_suite.pb.h"

using openscreen::cast::proto::DeviceCertTest;
using openscreen::cast::proto::DeviceCertTestSuite;

namespace cast_certificate {
namespace {

// Indicates the expected result of test step's verification.
enum TestStepResult {
  RESULT_SUCCESS,
  RESULT_FAIL,
};

// Verifies that the provided certificate chain is valid at the specified time
// and chains up to a trust anchor.
bool TestVerifyCertificate(TestStepResult expected_result,
                           const std::vector<std::string>& certificate_chain,
                           const base::Time& time) {
  std::unique_ptr<CertVerificationContext> context;
  CastDeviceCertPolicy policy;
  CastCertError result =
      VerifyDeviceCert(certificate_chain, time, &context, &policy, nullptr,
                       nullptr, CRLPolicy::CRL_OPTIONAL);
  bool success = result == CastCertError::OK;
  if (expected_result != RESULT_SUCCESS) {
    success = !success;
  }
  EXPECT_TRUE(success);
  return success;
}

// Verifies that the provided Cast CRL is signed by a trusted issuer
// and that the CRL can be parsed successfully.
// The validity of the CRL is also checked at the specified time.
bool TestVerifyCRL(TestStepResult expected_result,
                   const std::string& crl_bundle,
                   const base::Time& time,
                   bssl::TrustStore* crl_trust_store) {
  std::unique_ptr<CastCRL> crl = ParseAndVerifyCRLUsingCustomTrustStore(
      crl_bundle, time, crl_trust_store, false /* is_fallback_crl */);

  bool success = crl != nullptr;
  if (expected_result != RESULT_SUCCESS) {
    success = !success;
  }
  EXPECT_TRUE(success);
  return success;
}

// Verifies that the certificate chain provided is not revoked according to
// the provided Cast CRL at |cert_time|.
// The provided CRL is verified at |crl_time|.
// If |crl_policy| is set to CRL_REQUIRED, then a valid Cast CRL must be
// provided. Otherwise, a missing CRL is be ignored.
bool TestVerifyRevocation(CastCertError expected_result,
                          const std::vector<std::string>& certificate_chain,
                          const std::string& crl_bundle,
                          const base::Time& crl_time,
                          const base::Time& cert_time,
                          CRLPolicy crl_policy,
                          bssl::TrustStore* crl_trust_store) {
  std::unique_ptr<CastCRL> crl;
  if (!crl_bundle.empty()) {
    crl = ParseAndVerifyCRLUsingCustomTrustStore(
        crl_bundle, crl_time, crl_trust_store, false /* is_fallback_crl */);
    EXPECT_NE(crl.get(), nullptr);
  }

  std::unique_ptr<CertVerificationContext> context;
  CastDeviceCertPolicy policy;
  CastCertError result =
      VerifyDeviceCert(certificate_chain, cert_time, &context, &policy,
                       crl.get(), nullptr, crl_policy);
  EXPECT_EQ(expected_result, result);
  return expected_result == result;
}

// Runs a single test case.
bool RunTest(const DeviceCertTest& test_case) {
  std::unique_ptr<testing::ScopedCastTrustStoreConfig> scoped_cast_trust_store =
      test_case.use_test_trust_anchors()
          ? testing::ScopedCastTrustStoreConfig::TestCertificates(
                "cast_test_root_ca.pem")
          : testing::ScopedCastTrustStoreConfig::BuiltInCertificates();
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

  base::Time cert_verification_time = testing::ConvertUnixTimestampSeconds(
      test_case.cert_verification_time_seconds());

  uint64_t crl_verify_time = test_case.crl_verification_time_seconds();
  base::Time crl_verification_time =
      testing::ConvertUnixTimestampSeconds(crl_verify_time);
  if (crl_verify_time == 0)
    crl_verification_time = cert_verification_time;

  std::string crl_bundle = test_case.crl_bundle();
  switch (test_case.expected_result()) {
    case openscreen::cast::proto::PATH_VERIFICATION_FAILED:
      return TestVerifyCertificate(RESULT_FAIL, certificate_chain,
                                   cert_verification_time);
    case openscreen::cast::proto::CRL_VERIFICATION_FAILED:
      return TestVerifyCRL(RESULT_FAIL, crl_bundle, crl_verification_time,
                           crl_trust_store.get());
    case openscreen::cast::proto::REVOCATION_CHECK_FAILED_WITHOUT_CRL:
      return TestVerifyCertificate(RESULT_SUCCESS, certificate_chain,
                                   cert_verification_time) &&
             TestVerifyCRL(RESULT_FAIL, crl_bundle, crl_verification_time,
                           crl_trust_store.get()) &&
             TestVerifyRevocation(
                 CastCertError::ERR_CRL_INVALID, certificate_chain, crl_bundle,
                 crl_verification_time, cert_verification_time,
                 CRLPolicy::CRL_REQUIRED, crl_trust_store.get());
    case openscreen::cast::proto::CRL_EXPIRED_AFTER_INITIAL_VERIFICATION:
    // Fall-through intended.
    case openscreen::cast::proto::REVOCATION_CHECK_FAILED:
      return TestVerifyCertificate(RESULT_SUCCESS, certificate_chain,
                                   cert_verification_time) &&
             TestVerifyCRL(RESULT_SUCCESS, crl_bundle, crl_verification_time,
                           crl_trust_store.get()) &&
             TestVerifyRevocation(
                 CastCertError::ERR_CERTS_REVOKED, certificate_chain,
                 crl_bundle, crl_verification_time, cert_verification_time,
                 CRLPolicy::CRL_OPTIONAL, crl_trust_store.get());
    case openscreen::cast::proto::SUCCESS:
      return (crl_bundle.empty() ||
              TestVerifyCRL(RESULT_SUCCESS, crl_bundle, crl_verification_time,
                            crl_trust_store.get())) &&
             TestVerifyCertificate(RESULT_SUCCESS, certificate_chain,
                                   cert_verification_time) &&
             TestVerifyRevocation(CastCertError::OK, certificate_chain,
                                  crl_bundle, crl_verification_time,
                                  cert_verification_time,
                                  !crl_bundle.empty() ? CRLPolicy::CRL_REQUIRED
                                                      : CRLPolicy::CRL_OPTIONAL,
                                  crl_trust_store.get());
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
      testing::GetCastCertificateDirectory().AppendASCII(test_suite_file_name),
      &testsuite_raw);

  DeviceCertTestSuite test_suite;
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

TEST(CastCertificateTest, TestSuite1) {
  RunTestSuite("testsuite/testsuite1.pb");
}

}  // namespace

}  // namespace cast_certificate
