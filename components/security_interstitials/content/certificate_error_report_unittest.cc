// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/certificate_error_report.h"

#include <set>
#include <string>

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/prefs/testing_pref_service.h"
#include "components/security_interstitials/content/cert_logger.pb.h"
#include "components/version_info/version_info.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/cert_verify_result.h"
#include "net/net_buildflags.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "net/cert/cert_verify_proc_android.h"
#endif

#if BUILDFLAG(IS_APPLE)
#include "net/cert/internal/trust_store_mac.h"
#endif

using net::SSLInfo;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

namespace {

const char kDummyHostname[] = "dummy.hostname.com";
const char kDummyFailureLog[] = "dummy failure log";
const char kTestCertFilename[] = "x509_verify_results.chain.pem";

const net::CertStatus kCertStatus =
    net::CERT_STATUS_COMMON_NAME_INVALID | net::CERT_STATUS_REVOKED;

const chrome_browser_ssl::CertLoggerRequest::CertError kFirstReportedCertError =
    chrome_browser_ssl::CertLoggerRequest::ERR_CERT_COMMON_NAME_INVALID;
const chrome_browser_ssl::CertLoggerRequest::CertError
    kSecondReportedCertError =
        chrome_browser_ssl::CertLoggerRequest::ERR_CERT_REVOKED;

// Whether to include an unverified certificate chain in the test
// SSLInfo. In production code, an unverified cert chain will not be
// present if the resource was loaded from cache.
enum UnverifiedCertChainStatus {
  INCLUDE_UNVERIFIED_CERT_CHAIN,
  EXCLUDE_UNVERIFIED_CERT_CHAIN
};

void GetTestSSLInfo(UnverifiedCertChainStatus unverified_cert_chain_status,
                    SSLInfo* info,
                    net::CertStatus cert_status) {
  info->cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), kTestCertFilename);
  ASSERT_TRUE(info->cert);
  if (unverified_cert_chain_status == INCLUDE_UNVERIFIED_CERT_CHAIN) {
    info->unverified_cert = net::ImportCertFromFile(
        net::GetTestCertsDirectory(), kTestCertFilename);
    ASSERT_TRUE(info->unverified_cert);
  }
  info->is_issued_by_known_root = true;
  info->cert_status = cert_status;
  info->pinning_failure_log = kDummyFailureLog;
}

std::string GetPEMEncodedChain() {
  std::string cert_data;
  std::vector<std::string> pem_certs;
  scoped_refptr<net::X509Certificate> cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), kTestCertFilename);
  if (!cert || !cert->GetPEMEncodedChain(&pem_certs)) {
    ADD_FAILURE();
    return cert_data;
  }
  for (const auto& pem_cert : pem_certs) {
    cert_data += pem_cert;
  }
  return cert_data;
}

void VerifyDeserializedReportSystemInfo(
    const chrome_browser_ssl::CertLoggerRequest& parsed) {
  ASSERT_TRUE(parsed.has_chrome_version());
  EXPECT_FALSE(parsed.chrome_version().empty());
  ASSERT_TRUE(parsed.has_os_type());
  EXPECT_FALSE(parsed.os_type().empty());
  ASSERT_TRUE(parsed.has_os_version());
  EXPECT_FALSE(parsed.os_version().empty());
  ASSERT_TRUE(parsed.has_hardware_model_name());
  // HardwareModelName() may return empty string on some platforms.
  ASSERT_TRUE(parsed.has_os_architecture());
  EXPECT_FALSE(parsed.os_architecture().empty());
  ASSERT_TRUE(parsed.has_process_architecture());
  EXPECT_FALSE(parsed.process_architecture().empty());
}

void VerifyErrorReportSerialization(
    const CertificateErrorReport& report,
    const SSLInfo& ssl_info,
    std::vector<chrome_browser_ssl::CertLoggerRequest::CertError> cert_errors) {
  std::string serialized_report;
  ASSERT_TRUE(report.Serialize(&serialized_report));

  chrome_browser_ssl::CertLoggerRequest deserialized_report;
  ASSERT_TRUE(deserialized_report.ParseFromString(serialized_report));
  EXPECT_EQ(kDummyHostname, deserialized_report.hostname());
  EXPECT_EQ(GetPEMEncodedChain(), deserialized_report.cert_chain());
  EXPECT_EQ(GetPEMEncodedChain(), deserialized_report.unverified_cert_chain());
  EXPECT_EQ(1, deserialized_report.pin().size());
  EXPECT_EQ(kDummyFailureLog, deserialized_report.pin().Get(0));
  EXPECT_EQ(ssl_info.is_issued_by_known_root,
            deserialized_report.is_issued_by_known_root());
  EXPECT_THAT(deserialized_report.cert_error(),
              UnorderedElementsAreArray(cert_errors));

  VerifyDeserializedReportSystemInfo(deserialized_report);
}

// Test that a serialized CertificateErrorReport can be deserialized as
// a CertLoggerRequest protobuf (which is the format that the receiving
// server expects it in) with the right data in it.
TEST(ErrorReportTest, SerializedReportAsProtobuf) {
  SSLInfo ssl_info;
  ASSERT_NO_FATAL_FAILURE(
      GetTestSSLInfo(INCLUDE_UNVERIFIED_CERT_CHAIN, &ssl_info, kCertStatus));
  CertificateErrorReport report_known(kDummyHostname, ssl_info);
  std::vector<chrome_browser_ssl::CertLoggerRequest::CertError> cert_errors;
  cert_errors.push_back(kFirstReportedCertError);
  cert_errors.push_back(kSecondReportedCertError);
  ASSERT_NO_FATAL_FAILURE(
      VerifyErrorReportSerialization(report_known, ssl_info, cert_errors));
  // Test that both values for |is_issued_by_known_root| are serialized
  // correctly.
  ssl_info.is_issued_by_known_root = false;
  CertificateErrorReport report_unknown(kDummyHostname, ssl_info);
  ASSERT_NO_FATAL_FAILURE(
      VerifyErrorReportSerialization(report_unknown, ssl_info, cert_errors));
}

TEST(ErrorReportTest, SerializedReportAsProtobufWithInterstitialInfo) {
  std::string serialized_report;
  SSLInfo ssl_info;
  // Use EXCLUDE_UNVERIFIED_CERT_CHAIN here to exercise the code path
  // where SSLInfo does not contain the unverified cert chain. (The test
  // above exercises the path where it does.)
  ASSERT_NO_FATAL_FAILURE(
      GetTestSSLInfo(EXCLUDE_UNVERIFIED_CERT_CHAIN, &ssl_info, kCertStatus));
  CertificateErrorReport report(kDummyHostname, ssl_info);

  const base::Time interstitial_time = base::Time::Now();
  report.SetInterstitialInfo(CertificateErrorReport::INTERSTITIAL_CLOCK,
                             CertificateErrorReport::USER_PROCEEDED,
                             CertificateErrorReport::INTERSTITIAL_OVERRIDABLE,
                             interstitial_time);

  ASSERT_TRUE(report.Serialize(&serialized_report));

  chrome_browser_ssl::CertLoggerRequest deserialized_report;
  ASSERT_TRUE(deserialized_report.ParseFromString(serialized_report));
  EXPECT_EQ(kDummyHostname, deserialized_report.hostname());
  EXPECT_EQ(GetPEMEncodedChain(), deserialized_report.cert_chain());
  EXPECT_EQ(std::string(), deserialized_report.unverified_cert_chain());
  EXPECT_EQ(1, deserialized_report.pin().size());
  EXPECT_EQ(kDummyFailureLog, deserialized_report.pin().Get(0));

  EXPECT_EQ(chrome_browser_ssl::CertLoggerInterstitialInfo::INTERSTITIAL_CLOCK,
            deserialized_report.interstitial_info().interstitial_reason());
  EXPECT_EQ(true, deserialized_report.interstitial_info().user_proceeded());
  EXPECT_EQ(true, deserialized_report.interstitial_info().overridable());
  EXPECT_EQ(ssl_info.is_issued_by_known_root,
            deserialized_report.is_issued_by_known_root());

  EXPECT_THAT(
      deserialized_report.cert_error(),
      UnorderedElementsAre(kFirstReportedCertError, kSecondReportedCertError));

  EXPECT_EQ(
      interstitial_time.ToDeltaSinceWindowsEpoch().InMicroseconds(),
      deserialized_report.interstitial_info().interstitial_created_time_usec());
}

// Test that a serialized report can be parsed.
TEST(ErrorReportTest, ParseSerializedReport) {
  std::string serialized_report;
  SSLInfo ssl_info;
  ASSERT_NO_FATAL_FAILURE(
      GetTestSSLInfo(INCLUDE_UNVERIFIED_CERT_CHAIN, &ssl_info, kCertStatus));
  CertificateErrorReport report(kDummyHostname, ssl_info);
  EXPECT_EQ(kDummyHostname, report.hostname());
  ASSERT_TRUE(report.Serialize(&serialized_report));

  CertificateErrorReport parsed;
  ASSERT_TRUE(parsed.InitializeFromString(serialized_report));
  EXPECT_EQ(report.hostname(), parsed.hostname());
}

// Check that CT errors are handled correctly.
TEST(ErrorReportTest, CertificateTransparencyError) {
  SSLInfo ssl_info;
  ASSERT_NO_FATAL_FAILURE(
      GetTestSSLInfo(INCLUDE_UNVERIFIED_CERT_CHAIN, &ssl_info,
                     net::CERT_STATUS_CERTIFICATE_TRANSPARENCY_REQUIRED));
  CertificateErrorReport report_known(kDummyHostname, ssl_info);
  std::vector<chrome_browser_ssl::CertLoggerRequest::CertError> cert_errors;
  cert_errors.push_back(chrome_browser_ssl::CertLoggerRequest::
                            ERR_CERTIFICATE_TRANSPARENCY_REQUIRED);
  ASSERT_NO_FATAL_FAILURE(
      VerifyErrorReportSerialization(report_known, ssl_info, cert_errors));
}

// Tests that information about network time querying is included in the
// report.
TEST(ErrorReportTest, NetworkTimeQueryingFeatureInfo) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  std::unique_ptr<network_time::FieldTrialTest> field_trial_test(
      new network_time::FieldTrialTest());
  field_trial_test->SetFeatureParams(
      true, 0.0, network_time::NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY,
      network_time::NetworkTimeTracker::ClockDriftSamples::NO_SAMPLES);

  scoped_refptr<network::TestSharedURLLoaderFactory> shared_url_loader_factory =
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>();

  TestingPrefServiceSimple pref_service;
  network_time::NetworkTimeTracker::RegisterPrefs(pref_service.registry());
  network_time::NetworkTimeTracker network_time_tracker(
      std::make_unique<base::DefaultClock>(),
      std::make_unique<base::DefaultTickClock>(), &pref_service,
      shared_url_loader_factory);

  // Serialize a report containing information about the network time querying
  // feature.
  SSLInfo ssl_info;
  ASSERT_NO_FATAL_FAILURE(
      GetTestSSLInfo(INCLUDE_UNVERIFIED_CERT_CHAIN, &ssl_info, kCertStatus));
  CertificateErrorReport report(kDummyHostname, ssl_info);
  report.AddNetworkTimeInfo(&network_time_tracker);
  std::string serialized_report;
  ASSERT_TRUE(report.Serialize(&serialized_report));

  // Check that the report contains the network time querying feature
  // information.
  chrome_browser_ssl::CertLoggerRequest parsed;
  ASSERT_TRUE(parsed.ParseFromString(serialized_report));
  EXPECT_TRUE(parsed.features_info()
                  .network_time_querying_info()
                  .network_time_queries_enabled());
  EXPECT_EQ(chrome_browser_ssl::CertLoggerFeaturesInfo::
                NetworkTimeQueryingInfo::NETWORK_TIME_FETCHES_ON_DEMAND_ONLY,
            parsed.features_info()
                .network_time_querying_info()
                .network_time_query_behavior());
}

TEST(ErrorReportTest, TestChromeChannelIncluded) {
  struct ChannelTestCase {
    version_info::Channel channel;
    chrome_browser_ssl::CertLoggerRequest::ChromeChannel expected_channel;
  } kTestCases[] = {
      {version_info::Channel::UNKNOWN,
       chrome_browser_ssl::CertLoggerRequest::CHROME_CHANNEL_UNKNOWN},
      {version_info::Channel::DEV,
       chrome_browser_ssl::CertLoggerRequest::CHROME_CHANNEL_DEV},
      {version_info::Channel::CANARY,
       chrome_browser_ssl::CertLoggerRequest::CHROME_CHANNEL_CANARY},
      {version_info::Channel::BETA,
       chrome_browser_ssl::CertLoggerRequest::CHROME_CHANNEL_BETA},
      {version_info::Channel::STABLE,
       chrome_browser_ssl::CertLoggerRequest::CHROME_CHANNEL_STABLE}};

  // Create a report, set its channel value and check if we
  // get back test_case.expected_channel.
  for (const ChannelTestCase& test_case : kTestCases) {
    SSLInfo ssl_info;
    ASSERT_NO_FATAL_FAILURE(
        GetTestSSLInfo(INCLUDE_UNVERIFIED_CERT_CHAIN, &ssl_info, kCertStatus));
    CertificateErrorReport report(kDummyHostname, ssl_info);

    report.AddChromeChannel(test_case.channel);
    std::string serialized_report;
    ASSERT_TRUE(report.Serialize(&serialized_report));

    chrome_browser_ssl::CertLoggerRequest parsed;
    ASSERT_TRUE(parsed.ParseFromString(serialized_report));
    EXPECT_EQ(test_case.expected_channel, parsed.chrome_channel());
  }
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
// Tests that the SetIsEnterpriseManaged() function populates
// is_enterprise_managed correctly on Windows, and that value is correctly
// extracted from the parsed report.
// These tests are OS specific because SetIsEnterpriseManaged is called only
// on the Windows and ChromeOS OS.
TEST(ErrorReportTest, TestIsEnterpriseManagedPopulatedOnWindows) {
  SSLInfo ssl_info;
  ASSERT_NO_FATAL_FAILURE(
      GetTestSSLInfo(INCLUDE_UNVERIFIED_CERT_CHAIN, &ssl_info, kCertStatus));
  CertificateErrorReport report(kDummyHostname, ssl_info);

  report.SetIsEnterpriseManaged(true);
  std::string serialized_report;
  ASSERT_TRUE(report.Serialize(&serialized_report));

  chrome_browser_ssl::CertLoggerRequest parsed;
  ASSERT_TRUE(parsed.ParseFromString(serialized_report));
  EXPECT_EQ(true, parsed.is_enterprise_managed());
}
#endif

#if BUILDFLAG(IS_ANDROID)
// Tests that information about the Android AIA fetching feature is included in
// the report.
TEST(ErrorReportTest, AndroidAIAFetchingFeatureEnabled) {
  SSLInfo ssl_info;
  ASSERT_NO_FATAL_FAILURE(
      GetTestSSLInfo(INCLUDE_UNVERIFIED_CERT_CHAIN, &ssl_info, kCertStatus));
  CertificateErrorReport report(kDummyHostname, ssl_info);
  std::string serialized_report;
  ASSERT_TRUE(report.Serialize(&serialized_report));
  chrome_browser_ssl::CertLoggerRequest parsed;
  ASSERT_TRUE(parsed.ParseFromString(serialized_report));
  EXPECT_EQ(
      chrome_browser_ssl::CertLoggerFeaturesInfo::ANDROID_AIA_FETCHING_ENABLED,
      parsed.features_info().android_aia_fetching_status());
}
#endif

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
const int64_t kTestChromeRootVersion = 24601;
#endif

TEST(ErrorReportTest, TrialDebugInfo) {
  scoped_refptr<net::X509Certificate> unverified_cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  scoped_refptr<net::X509Certificate> chain1 =
      net::CreateCertificateChainFromFile(net::GetTestCertsDirectory(),
                                          "x509_verify_results.chain.pem",
                                          net::X509Certificate::FORMAT_AUTO);
  scoped_refptr<net::X509Certificate> chain2 =
      net::CreateCertificateChainFromFile(net::GetTestCertsDirectory(),
                                          "multi-root-chain1.pem",
                                          net::X509Certificate::FORMAT_AUTO);
  net::CertVerifyResult primary_result;
  primary_result.verified_cert = chain1;
  net::CertVerifyResult trial_result;
  trial_result.verified_cert = chain2;

  cert_verifier::mojom::CertVerifierDebugInfoPtr debug_info =
      cert_verifier::mojom::CertVerifierDebugInfo::New();
#if BUILDFLAG(IS_APPLE)
  debug_info->mac_platform_debug_info =
      cert_verifier::mojom::MacPlatformVerifierDebugInfo::New();
  debug_info->mac_platform_debug_info->trust_result = 1;
  debug_info->mac_platform_debug_info->result_code = 20;
  cert_verifier::mojom::MacCertEvidenceInfoPtr info =
      cert_verifier::mojom::MacCertEvidenceInfo::New();
  info->status_bits = 30;
  info->status_codes = {40, 41};
  debug_info->mac_platform_debug_info->status_chain.push_back(std::move(info));
  info = cert_verifier::mojom::MacCertEvidenceInfo::New();
  info->status_bits = 50;
  info->status_codes = {};
  debug_info->mac_platform_debug_info->status_chain.push_back(std::move(info));
  info = cert_verifier::mojom::MacCertEvidenceInfo::New();
  info->status_bits = 70;
  info->status_codes = {80, 81, 82};
  debug_info->mac_platform_debug_info->status_chain.push_back(std::move(info));

  debug_info->mac_combined_trust_debug_info =
      net::TrustStoreMac::TRUST_SETTINGS_DICT_CONTAINS_APPLICATION |
      net::TrustStoreMac::TRUST_SETTINGS_DICT_CONTAINS_RESULT;
  debug_info->mac_trust_impl =
      cert_verifier::mojom::CertVerifierDebugInfo::MacTrustImplType::kSimple;
#endif
#if BUILDFLAG(IS_WIN)
  debug_info->win_platform_debug_info =
      cert_verifier::mojom::WinPlatformVerifierDebugInfo::New();
  debug_info->win_platform_debug_info->authroot_this_update =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(8675309));
  debug_info->win_platform_debug_info->authroot_sequence_number = {
      'J', 'E', 'N', 'N', 'Y'};
#endif
#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  debug_info->chrome_root_store_debug_info =
      cert_verifier::mojom::ChromeRootStoreDebugInfo::New();
  debug_info->chrome_root_store_debug_info->chrome_root_store_version =
      kTestChromeRootVersion;
#endif

  base::Time time = base::Time::Now();
  debug_info->trial_verification_time = time;
  debug_info->trial_der_verification_time = "it's just a string";

  CertificateErrorReport report("example.com", *unverified_cert, false, false,
                                false, false, "ocsp", "sct", primary_result,
                                trial_result, std::move(debug_info));
  std::string serialized_report;
  ASSERT_TRUE(report.Serialize(&serialized_report));
  chrome_browser_ssl::CertLoggerRequest parsed;
  ASSERT_TRUE(parsed.ParseFromString(serialized_report));
  ASSERT_TRUE(parsed.has_features_info());
  ASSERT_TRUE(parsed.features_info().has_trial_verification_info());
  const chrome_browser_ssl::TrialVerificationInfo& trial_info =
      parsed.features_info().trial_verification_info();
  ASSERT_TRUE(trial_info.has_stapled_ocsp());
  EXPECT_EQ("ocsp", trial_info.stapled_ocsp());
  ASSERT_TRUE(trial_info.has_sct_list());
  EXPECT_EQ("sct", trial_info.sct_list());

  VerifyDeserializedReportSystemInfo(parsed);

#if BUILDFLAG(IS_APPLE)
  ASSERT_TRUE(trial_info.has_mac_platform_debug_info());
  EXPECT_EQ(1U, trial_info.mac_platform_debug_info().trust_result());
  EXPECT_EQ(20, trial_info.mac_platform_debug_info().result_code());
  ASSERT_EQ(3, trial_info.mac_platform_debug_info().status_chain_size());
  EXPECT_EQ(30U,
            trial_info.mac_platform_debug_info().status_chain(0).status_bits());
  ASSERT_EQ(
      2,
      trial_info.mac_platform_debug_info().status_chain(0).status_codes_size());
  EXPECT_EQ(
      40, trial_info.mac_platform_debug_info().status_chain(0).status_codes(0));
  EXPECT_EQ(
      41, trial_info.mac_platform_debug_info().status_chain(0).status_codes(1));
  EXPECT_EQ(50U,
            trial_info.mac_platform_debug_info().status_chain(1).status_bits());
  EXPECT_EQ(
      0,
      trial_info.mac_platform_debug_info().status_chain(1).status_codes_size());
  EXPECT_EQ(70U,
            trial_info.mac_platform_debug_info().status_chain(2).status_bits());
  ASSERT_EQ(
      3,
      trial_info.mac_platform_debug_info().status_chain(2).status_codes_size());
  EXPECT_EQ(
      80, trial_info.mac_platform_debug_info().status_chain(2).status_codes(0));
  EXPECT_EQ(
      81, trial_info.mac_platform_debug_info().status_chain(2).status_codes(1));
  EXPECT_EQ(
      82, trial_info.mac_platform_debug_info().status_chain(2).status_codes(2));

  ASSERT_EQ(2, trial_info.mac_combined_trust_debug_info_size());
  EXPECT_EQ(chrome_browser_ssl::TrialVerificationInfo::
                MAC_TRUST_SETTINGS_DICT_CONTAINS_APPLICATION,
            trial_info.mac_combined_trust_debug_info()[0]);
  EXPECT_EQ(chrome_browser_ssl::TrialVerificationInfo::
                MAC_TRUST_SETTINGS_DICT_CONTAINS_RESULT,
            trial_info.mac_combined_trust_debug_info()[1]);
  EXPECT_TRUE(trial_info.has_mac_trust_impl());
  EXPECT_EQ(chrome_browser_ssl::TrialVerificationInfo::MAC_TRUST_IMPL_SIMPLE,
            trial_info.mac_trust_impl());
#else
  EXPECT_FALSE(trial_info.has_mac_platform_debug_info());
  EXPECT_EQ(0, trial_info.mac_combined_trust_debug_info_size());
  EXPECT_FALSE(trial_info.has_mac_trust_impl());
#endif

#if BUILDFLAG(IS_WIN)
  ASSERT_TRUE(trial_info.has_win_platform_debug_info());
  EXPECT_EQ(
      8675309,
      trial_info.win_platform_debug_info().authroot_this_update_time_usec());
  EXPECT_EQ("JENNY",
            trial_info.win_platform_debug_info().authroot_sequence_number());
#else
  EXPECT_FALSE(trial_info.has_win_platform_debug_info());
#endif

#if BUILDFLAG(CHROME_ROOT_STORE_SUPPORTED)
  ASSERT_TRUE(trial_info.has_chrome_root_store_debug_info());
  EXPECT_EQ(
      kTestChromeRootVersion,
      trial_info.chrome_root_store_debug_info().chrome_root_store_version());
#else
  EXPECT_FALSE(trial_info.has_chrome_root_store_debug_info());
#endif

  ASSERT_TRUE(trial_info.has_trial_verification_time_usec());
  EXPECT_EQ(time.ToDeltaSinceWindowsEpoch().InMicroseconds(),
            trial_info.trial_verification_time_usec());
  ASSERT_TRUE(trial_info.has_trial_der_verification_time());
  EXPECT_EQ("it's just a string", trial_info.trial_der_verification_time());
}
#endif  // BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)

}  // namespace
