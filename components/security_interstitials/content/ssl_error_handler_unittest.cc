// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_interstitials/content/ssl_error_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "components/captive_portal/content/captive_portal_service.h"
#include "components/captive_portal/core/buildflags.h"
#include "components/captive_portal/core/captive_portal_testing_utils.h"
#include "components/embedder_support/pref_names.h"
#include "components/network_time/network_time_test_utils.h"
#include "components/network_time/network_time_tracker.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/security_interstitials/content/common_name_mismatch_handler.h"
#include "components/security_interstitials/content/ssl_error_assistant.h"
#include "components/security_interstitials/content/ssl_error_assistant.pb.h"
#include "components/security_interstitials/core/ssl_error_options_mask.h"
#include "components/security_interstitials/core/ssl_error_ui.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "net/base/net_errors.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "net/http/http_response_headers.h"
#include "net/ssl/ssl_info.h"
#include "net/test/cert_test_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const net::SHA256HashValue kCertPublicKeyHashValue = {{0x01, 0x02}};

const char kOkayCertName[] = "ok_cert.pem";

const uint32_t kLargeVersionId = 0xFFFFFFu;

// These certificates are self signed certificates with relevant issuer common
// names generated using the following openssl command:
//  openssl req -new -x509 -keyout server.pem -out server.pem -days 365 -nodes

// Common name: "Misconfigured Firewall_4GHPOS5412EF"
// Organization name: "Misconfigured Firewall"
const char kMisconfiguredFirewallCert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIEKTCCAxGgAwIBAgIJAOxA1g2otzdHMA0GCSqGSIb3DQEBCwUAMIGqMQswCQYD\n"
    "VQQGEwJVUzETMBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQGA1UEBwwNU2FuIEZyYW5j\n"
    "aXNjbzEfMB0GA1UECgwWTWlzY29uZmlndXJlZCBGaXJld2FsbDEsMCoGA1UEAwwj\n"
    "TWlzY29uZmlndXJlZCBGaXJld2FsbF80R0hQT1M1NDEyRUYxHzAdBgkqhkiG9w0B\n"
    "CQEWEHRlc3RAZXhhbXBsZS5jb20wHhcNMTcwODE4MjM1MjI4WhcNMTgwODE4MjM1\n"
    "MjI4WjCBqjELMAkGA1UEBhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExFjAUBgNV\n"
    "BAcMDVNhbiBGcmFuY2lzY28xHzAdBgNVBAoMFk1pc2NvbmZpZ3VyZWQgRmlyZXdh\n"
    "bGwxLDAqBgNVBAMMI01pc2NvbmZpZ3VyZWQgRmlyZXdhbGxfNEdIUE9TNTQxMkVG\n"
    "MR8wHQYJKoZIhvcNAQkBFhB0ZXN0QGV4YW1wbGUuY29tMIIBIjANBgkqhkiG9w0B\n"
    "AQEFAAOCAQ8AMIIBCgKCAQEAtxh4PZ9dbqeXubutRBFSL4FschunDX/vRFzhlQdz\n"
    "3fqzIfmN2PjvwBsoX1oDaWdTTefCLad7pX08UVyX2pS0UeqYwUJL+ihXuupW0pBV\n"
    "M2VZ/soDgze7Vl9dUU43NLoODOzwvKt92QdyfS7toPEEmwFLrI4/UnzxX+QlS8qq\n"
    "naWD5ny2XZOZdNizBX1UQlvkvfYJM0wUmBZ/VUj/QQxxNHZaEBcl64t3h5jHiq1c\n"
    "gWDgp0zeYy+PbJk/LMSvF64qqMFDtujUQcniYC6HwWJ9YT7PFX2b7X9Mq4b3gtpV\n"
    "6jGXXUJqg+SfLW7XisZcWVMfHZDaVfdd35vNm61XY4sg1wIDAQABo1AwTjAdBgNV\n"
    "HQ4EFgQUmUhF2RL+A4QAEel9JiEYNbPyU+AwHwYDVR0jBBgwFoAUmUhF2RL+A4QA\n"
    "Eel9JiEYNbPyU+AwDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAO+kk\n"
    "Uin9uKD2iTkUtuoAckt+kcctmvHcP3zLe+J0m25f9HOyrjhutgIYZDwt2LiAnOTA\n"
    "CQHg3t5YyDQHove39M6I1vWhoAy9jOi0Qn+lTKkVu5H4g5ZiauO3coneqFw/dPe+\n"
    "kYye/bPKV4jNlhEYXF5+Pa7PYde0sxf7AmlDJb9NZh01xRKNFt6ScDpirhJIFdzg\n"
    "ZKram+yJyIbcZI+yd7mjzu9dSCS0NbnsZDL7xqThFFZsbhZyO98kzdDS+crip6y5\n"
    "rz3+AJpJvlGcf898Y4ibAPmeX62j6pug55TGfAdsqSVUiaQX1HcwwbmlSOYrhYTm\n"
    "lMEx5QP9TqgGU0nGwQ==\n"
    "-----END CERTIFICATE-----";

// Common name: None
// Organization name: None
const char kCertWithoutOrganizationOrCommonName[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDzzCCAregAwIBAgIJAJfHNOMLXbc4MA0GCSqGSIb3DQEBCwUAMH4xCzAJBgNV\n"
    "BAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRYwFAYDVQQHDA1TYW4gRnJhbmNp\n"
    "c2NvMSEwHwYDVQQKDBhJbnRlcm5ldCBXaWRnaXRzIFB0eSBMdGQxHzAdBgkqhkiG\n"
    "9w0BCQEWEHRlc3RAZXhhbXBsZS5jb20wHhcNMTcwODE5MDAwNTMyWhcNMTgwODE5\n"
    "MDAwNTMyWjB+MQswCQYDVQQGEwJVUzETMBEGA1UECAwKQ2FsaWZvcm5pYTEWMBQG\n"
    "A1UEBwwNU2FuIEZyYW5jaXNjbzEhMB8GA1UECgwYSW50ZXJuZXQgV2lkZ2l0cyBQ\n"
    "dHkgTHRkMR8wHQYJKoZIhvcNAQkBFhB0ZXN0QGV4YW1wbGUuY29tMIIBIjANBgkq\n"
    "hkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA28iX7cIS5XS+hU/0OreJXfVmEWDPVRX1\n"
    "n05AlX+ETRunnYevZOAhbSFuUeJi2cGgW4cpD6fGKrf05PpNM9GQ4yswIPlVsemR\n"
    "ickSmg8vVemPs/Hz3y0dYnRoTwzzVESh4OIVGe+rrhCUdWVHE+/HOdmHAXoBI6m1\n"
    "OhN2GgtvnEEMYzTaMRGNqb5VhRKYHwLNp8zqLtrHIbo61mi8Wl7E4NZdaVk4cTNK\n"
    "w93Y8RqlwzzpbWT9RH74JPCM+wSg0rCK+h59sa86W4yPvhXyYIGXM8WhWkMW68Ej\n"
    "jqfE0lQlEuxKPeCYZn6oC+AVRLxHCwncVxZaUtGUovMzBdV3WzsLPwIDAQABo1Aw\n"
    "TjAdBgNVHQ4EFgQUlkC11ZD66sKrb25g4mH4sob4e3MwHwYDVR0jBBgwFoAUlkC1\n"
    "1ZD66sKrb25g4mH4sob4e3MwDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQsFAAOC\n"
    "AQEAUHQZmeZdAV86TIWviPNWNqhPD+/OEGnOwgjUrBmSrQkc5hPZUhQ8the7ewNE\n"
    "V/eGjDNF72tiQqPQP7Zrhdf7i1p1Q3ufcDHpOOFbEdKd6m2DeCLg83jOLqLr/jTB\n"
    "CC7GyyWOyt+CFVRGC0yovSl3+Vxaso6DZjelO3IP5K7bT5U1f3cUZnYTpYfslh1t\n"
    "dUmxh9/MaKxnRaHkr0HDVGpWS4ZMoZUyyC6D9ZfCQ5aGJJubQEPxADc2tXHXOL73\n"
    "dspwZ8CTOlcXnfdeRIjvgxnMZLax+OFEMJdY8sgyrI9c+rk2EfOUj5JVqFDvcsYy\n"
    "ejdBhjdieIv5dTbSjIXz+ljOOA==\n"
    "-----END CERTIFICATE-----";

// Runs |quit_closure| on the UI thread once a URL request has been
// seen. Returns a request that hangs.
std::unique_ptr<net::test_server::HttpResponse> WaitForRequest(
    base::OnceClosure quit_closure,
    const net::test_server::HttpRequest& request) {
  content::GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                               std::move(quit_closure));
  return std::make_unique<net::test_server::HungResponse>();
}

class TestSSLErrorHandler : public SSLErrorHandler {
 public:
  TestSSLErrorHandler(
      std::unique_ptr<Delegate> delegate,
      content::WebContents* web_contents,
      int cert_error,
      const net::SSLInfo& ssl_info,
      network_time::NetworkTimeTracker* network_time_tracker,
      const GURL& request_url,
      captive_portal::CaptivePortalService* captive_portal_service)
      : SSLErrorHandler(std::move(delegate),
                        web_contents,
                        cert_error,
                        ssl_info,
                        network_time_tracker,
                        captive_portal_service,
                        request_url) {}

  using SSLErrorHandler::StartHandlingError;
};

class TestSSLErrorHandlerDelegate : public SSLErrorHandler::Delegate {
 public:
  TestSSLErrorHandlerDelegate(content::WebContents* web_contents,
                              const net::SSLInfo& ssl_info)
      : captive_portal_checked_(false),
        os_reports_captive_portal_(false),
        suggested_url_exists_(false),
        suggested_url_checked_(false),
        ssl_interstitial_shown_(false),
        bad_clock_interstitial_shown_(false),
        captive_portal_interstitial_shown_(false),
        mitm_software_interstitial_shown_(false),
        blocked_interception_interstitial_shown_(false),
        redirected_to_suggested_url_(false),
        is_overridable_error_(true),
        has_blocked_interception_(false) {}

  TestSSLErrorHandlerDelegate(const TestSSLErrorHandlerDelegate&) = delete;
  TestSSLErrorHandlerDelegate& operator=(const TestSSLErrorHandlerDelegate&) =
      delete;

  void SendSuggestedUrlCheckResult(
      const CommonNameMismatchHandler::SuggestedUrlCheckResult& result,
      const GURL& suggested_url) {
    std::move(suggested_url_callback_).Run(result, suggested_url);
  }

  int captive_portal_checked() const { return captive_portal_checked_; }
  int ssl_interstitial_shown() const { return ssl_interstitial_shown_; }
  int captive_portal_interstitial_shown() const {
    return captive_portal_interstitial_shown_;
  }
  int mitm_software_interstitial_shown() const {
    return mitm_software_interstitial_shown_;
  }
  bool bad_clock_interstitial_shown() const {
    return bad_clock_interstitial_shown_;
  }
  bool blocked_interception_interstitial_shown() const {
    return blocked_interception_interstitial_shown_;
  }
  bool suggested_url_checked() const { return suggested_url_checked_; }
  bool redirected_to_suggested_url() const {
    return redirected_to_suggested_url_;
  }

  void set_suggested_url_exists() { suggested_url_exists_ = true; }
  void set_non_overridable_error() { is_overridable_error_ = false; }
  void set_os_reports_captive_portal() { os_reports_captive_portal_ = true; }
  void set_has_blocked_interception() { has_blocked_interception_ = true; }

  void ClearSeenOperations() {
    captive_portal_checked_ = false;
    os_reports_captive_portal_ = false;
    suggested_url_exists_ = false;
    suggested_url_checked_ = false;
    ssl_interstitial_shown_ = false;
    bad_clock_interstitial_shown_ = false;
    captive_portal_interstitial_shown_ = false;
    mitm_software_interstitial_shown_ = false;
    redirected_to_suggested_url_ = false;
    has_blocked_interception_ = false;
  }

 private:
  void CheckForCaptivePortal() override { captive_portal_checked_ = true; }

  bool DoesOSReportCaptivePortal() override {
    return os_reports_captive_portal_;
  }

  bool GetSuggestedUrl(const std::vector<std::string>& dns_names,
                       GURL* suggested_url) const override {
    if (!suggested_url_exists_)
      return false;
    *suggested_url = GURL("www.example.com");
    return true;
  }

  void ShowSSLInterstitial(const GURL& support_url = GURL()) override {
    ssl_interstitial_shown_ = true;
  }

  void ShowBadClockInterstitial(const base::Time& now,
                                ssl_errors::ClockState clock_state) override {
    bad_clock_interstitial_shown_ = true;
  }

  void ShowCaptivePortalInterstitial(const GURL& landing_url) override {
    captive_portal_interstitial_shown_ = true;
  }

  void ShowMITMSoftwareInterstitial(
      const std::string& mitm_software_name) override {
    mitm_software_interstitial_shown_ = true;
  }

  void ShowBlockedInterceptionInterstitial() override {
    blocked_interception_interstitial_shown_ = true;
  }

  void CheckSuggestedUrl(
      const GURL& suggested_url,
      CommonNameMismatchHandler::CheckUrlCallback callback) override {
    DCHECK(suggested_url_callback_.is_null());
    suggested_url_checked_ = true;
    suggested_url_callback_ = std::move(callback);
  }

  void NavigateToSuggestedURL(const GURL& suggested_url) override {
    redirected_to_suggested_url_ = true;
  }

  bool IsErrorOverridable() const override { return is_overridable_error_; }

  void ReportNetworkConnectivity(base::OnceClosure callback) override {}

  bool HasBlockedInterception() const override {
    return has_blocked_interception_;
  }

  bool captive_portal_checked_;
  bool os_reports_captive_portal_;
  bool suggested_url_exists_;
  bool suggested_url_checked_;
  bool ssl_interstitial_shown_;
  bool bad_clock_interstitial_shown_;
  bool captive_portal_interstitial_shown_;
  bool mitm_software_interstitial_shown_;
  bool blocked_interception_interstitial_shown_;
  bool redirected_to_suggested_url_;
  bool is_overridable_error_;
  bool has_blocked_interception_;
  CommonNameMismatchHandler::CheckUrlCallback suggested_url_callback_;
};

}  // namespace

// A class to test name mismatch errors. Creates an error handler with a name
// mismatch error.
class SSLErrorHandlerNameMismatchTest
    : public content::RenderViewHostTestHarness {
 public:
  SSLErrorHandlerNameMismatchTest() = default;

  SSLErrorHandlerNameMismatchTest(const SSLErrorHandlerNameMismatchTest&) =
      delete;
  SSLErrorHandlerNameMismatchTest& operator=(
      const SSLErrorHandlerNameMismatchTest&) = delete;

  ~SSLErrorHandlerNameMismatchTest() override = default;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    SSLErrorHandler::ResetConfigForTesting();
    SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());
    ssl_info_.cert = GetCertificate();
    ssl_info_.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
    ssl_info_.public_key_hashes.push_back(
        net::HashValue(kCertPublicKeyHashValue));

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
    pref_service_.registry()->RegisterBooleanPref(
        embedder_support::kAlternateErrorPagesEnabled, true);
    captive_portal_service_ =
        std::make_unique<captive_portal::CaptivePortalService>(
            web_contents()->GetBrowserContext(), &pref_service_);
#endif

    delegate_ = new TestSSLErrorHandlerDelegate(web_contents(), ssl_info_);
    error_handler_ = std::make_unique<TestSSLErrorHandler>(
        std::unique_ptr<SSLErrorHandler::Delegate>(delegate_), web_contents(),
        net::MapCertStatusToNetError(ssl_info_.cert_status), ssl_info_,
        /*network_time_tracker=*/nullptr, GURL() /*request_url*/,
        captive_portal_service_.get());
  }

  void TearDown() override {
    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    captive_portal_service_.reset();
    error_handler_.reset(nullptr);
    SSLErrorHandler::ResetConfigForTesting();
    content::RenderViewHostTestHarness::TearDown();
  }

  TestSSLErrorHandler* error_handler() { return error_handler_.get(); }
  TestSSLErrorHandlerDelegate* delegate() { return delegate_; }

  const net::SSLInfo& ssl_info() { return ssl_info_; }

 private:
  // Returns a certificate for the test. Virtual to allow derived fixtures to
  // use a certificate with different characteristics.
  virtual scoped_refptr<net::X509Certificate> GetCertificate() {
    return net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                   "subjectAltName_www_example_com.pem");
  }

  net::SSLInfo ssl_info_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<captive_portal::CaptivePortalService> captive_portal_service_;
  std::unique_ptr<TestSSLErrorHandler> error_handler_;
  raw_ptr<TestSSLErrorHandlerDelegate, DanglingUntriaged> delegate_;
};

// A class to test name mismatch errors, where the certificate lacks a
// SubjectAltName. Creates an error handler with a name mismatch error.
class SSLErrorHandlerNameMismatchNoSANTest
    : public SSLErrorHandlerNameMismatchTest {
 public:
  SSLErrorHandlerNameMismatchNoSANTest() = default;

  SSLErrorHandlerNameMismatchNoSANTest(
      const SSLErrorHandlerNameMismatchNoSANTest&) = delete;
  SSLErrorHandlerNameMismatchNoSANTest& operator=(
      const SSLErrorHandlerNameMismatchNoSANTest&) = delete;

 private:
  // Return a certificate that contains no SubjectAltName field.
  scoped_refptr<net::X509Certificate> GetCertificate() override {
    return net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  }
};

// A class to test the captive portal certificate list feature. Creates an error
// handler with a name mismatch error by default. The error handler can be
// recreated by calling ResetErrorHandler() with an appropriate cert status.
class SSLErrorAssistantProtoTest : public content::RenderViewHostTestHarness {
 public:
  SSLErrorAssistantProtoTest(const SSLErrorAssistantProtoTest&) = delete;
  SSLErrorAssistantProtoTest& operator=(const SSLErrorAssistantProtoTest&) =
      delete;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();

    pref_service_.registry()->RegisterBooleanPref(
        embedder_support::kAlternateErrorPagesEnabled, true);

    SSLErrorHandler::ResetConfigForTesting();
    SSLErrorHandler::SetErrorAssistantProto(
        SSLErrorAssistant::GetErrorAssistantProtoFromResourceBundle());

    SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());
    ResetErrorHandlerFromFile(kOkayCertName,
                              net::CERT_STATUS_COMMON_NAME_INVALID);
  }

  void TearDown() override {
    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    captive_portal_service_.reset();
    error_handler_.reset(nullptr);
    SSLErrorHandler::ResetConfigForTesting();
    content::RenderViewHostTestHarness::TearDown();
  }

  TestSSLErrorHandler* error_handler() { return error_handler_.get(); }
  TestSSLErrorHandlerDelegate* delegate() { return delegate_; }

  const net::SSLInfo& ssl_info() { return ssl_info_; }

 protected:
  SSLErrorAssistantProtoTest() = default;
  ~SSLErrorAssistantProtoTest() override = default;

  void ResetErrorHandlerFromString(const std::string& cert_data,
                                   net::CertStatus cert_status) {
    net::CertificateList certs =
        net::X509Certificate::CreateCertificateListFromBytes(
            base::as_bytes(base::make_span(cert_data)),
            net::X509Certificate::FORMAT_AUTO);
    ASSERT_FALSE(certs.empty());
    ResetErrorHandler(certs[0], cert_status);
  }

  void ResetErrorHandlerFromFile(const std::string& cert_name,
                                 net::CertStatus cert_status) {
    ResetErrorHandler(
        net::ImportCertFromFile(net::GetTestCertsDirectory(), cert_name),
        cert_status);
  }

  // Set up an error assistant proto with mock captive portal hash data and
  // begin handling the certificate error.
  void RunCaptivePortalTest() {
    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    EXPECT_EQ(1u, ssl_info().public_key_hashes.size());

    auto config_proto =
        std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
    config_proto->set_version_id(kLargeVersionId);

    config_proto->add_captive_portal_cert()->set_sha256_hash(
        "sha256/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    config_proto->add_captive_portal_cert()->set_sha256_hash(
        ssl_info().public_key_hashes[0].ToString());
    config_proto->add_captive_portal_cert()->set_sha256_hash(
        "sha256/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");
    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));

    error_handler()->StartHandlingError();
  }

  void TestNoCaptivePortalInterstitial() {
    base::HistogramTester histograms;

    RunCaptivePortalTest();

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
    // On platforms where captive portal detection is enabled, timer should
    // start for captive portal detection.
    EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
    EXPECT_TRUE(delegate()->captive_portal_checked());
    EXPECT_FALSE(delegate()->ssl_interstitial_shown());
    EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());

    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());

    // Captive portal should be checked on non-Android platforms.
    EXPECT_TRUE(delegate()->captive_portal_checked());
    EXPECT_TRUE(delegate()->ssl_interstitial_shown());
    EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());
#else
    // When there is no custom captive portal detection logic, the timer should
    // not start and an SSL interstitial should be shown immediately.
    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    EXPECT_FALSE(delegate()->captive_portal_checked());
    EXPECT_TRUE(delegate()->ssl_interstitial_shown());
    EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());

    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    EXPECT_FALSE(delegate()->captive_portal_checked());
    EXPECT_TRUE(delegate()->ssl_interstitial_shown());
    EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());
#endif

    // Check that the histogram for the captive portal cert was recorded.
    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  }

  // Set up a mock SSL Error Assistant config with regexes that match the
  // outdated antivirus and misconfigured firewall certificate.
  void InitMITMSoftwareList() {
    auto config_proto =
        std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
    config_proto->set_version_id(kLargeVersionId);

    chrome_browser_ssl::MITMSoftware* filter =
        config_proto->add_mitm_software();
    filter->set_name("Misconfigured Firewall");
    filter->set_issuer_common_name_regex("Misconfigured Firewall_[A-Z0-9]+");
    filter->set_issuer_organization_regex("Misconfigured Firewall");

    SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  }

  void TestMITMSoftwareInterstitial() {
    base::HistogramTester histograms;

    delegate()->set_non_overridable_error();
    error_handler()->StartHandlingError();
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(delegate()->ssl_interstitial_shown());
    EXPECT_TRUE(delegate()->mitm_software_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());

    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 0);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 0);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL, 1);
  }

  void TestNoMITMSoftwareInterstitial() {
    base::HistogramTester histograms;

    delegate()->set_non_overridable_error();
    error_handler()->StartHandlingError();
    base::RunLoop().RunUntilIdle();

    EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
    EXPECT_TRUE(delegate()->ssl_interstitial_shown());
    EXPECT_FALSE(delegate()->mitm_software_interstitial_shown());
    EXPECT_FALSE(delegate()->suggested_url_checked());

    histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                2);
    histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                                 SSLErrorHandler::HANDLE_ALL, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
    histograms.ExpectBucketCount(
        SSLErrorHandler::GetHistogramNameForTesting(),
        SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL, 0);
  }

 private:
  void ResetErrorHandler(scoped_refptr<net::X509Certificate> cert,
                         net::CertStatus cert_status) {
    ssl_info_.Reset();
    ssl_info_.cert = cert;
    ssl_info_.cert_status = cert_status;
    ssl_info_.public_key_hashes.push_back(
        net::HashValue(kCertPublicKeyHashValue));

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)
    captive_portal_service_ =
        std::make_unique<captive_portal::CaptivePortalService>(
            web_contents()->GetBrowserContext(), &pref_service_);
#endif

    delegate_ = new TestSSLErrorHandlerDelegate(web_contents(), ssl_info_);
    error_handler_ = std::make_unique<TestSSLErrorHandler>(
        std::unique_ptr<SSLErrorHandler::Delegate>(delegate_), web_contents(),
        net::MapCertStatusToNetError(ssl_info_.cert_status), ssl_info_,
        /*network_time_tracker=*/nullptr, GURL() /*request_url*/,
        captive_portal_service_.get());
  }

  net::SSLInfo ssl_info_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<captive_portal::CaptivePortalService> captive_portal_service_;
  std::unique_ptr<TestSSLErrorHandler> error_handler_;
  raw_ptr<TestSSLErrorHandlerDelegate, DanglingUntriaged> delegate_;
};

class SSLErrorAssistantProtoMITMSoftwareEnabledTest
    : public SSLErrorAssistantProtoTest {
 public:
  SSLErrorAssistantProtoMITMSoftwareEnabledTest() {
    scoped_feature_list_.InitAndEnableFeature(kMITMSoftwareInterstitial);
  }

 private:
  // This should only be accessed from a test's constructor, to avoid tsan data
  // races with threads kicked off by RenderViewHostTestHarness::SetUp().
  base::test::ScopedFeatureList scoped_feature_list_;
};

class SSLErrorAssistantProtoMITMSoftwareDisabledTest
    : public SSLErrorAssistantProtoTest {
 public:
  SSLErrorAssistantProtoMITMSoftwareDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(kMITMSoftwareInterstitial);
  }

 private:
  // This should only be accessed from a test's constructor, to avoid tsan data
  // races with threads kicked off by RenderViewHostTestHarness::SetUp().
  base::test::ScopedFeatureList scoped_feature_list_;
};

class SSLErrorHandlerDateInvalidTest
    : public content::RenderViewHostTestHarness {
 public:
  SSLErrorHandlerDateInvalidTest()
      : content::RenderViewHostTestHarness(
            content::BrowserTaskEnvironment::REAL_IO_THREAD),
        field_trial_test_(new network_time::FieldTrialTest()),
        clock_(new base::SimpleTestClock),
        tick_clock_(new base::SimpleTestTickClock),
        test_server_(new net::EmbeddedTestServer) {
    network_time::NetworkTimeTracker::RegisterPrefs(pref_service_.registry());

    field_trial_test()->SetFeatureParams(
        false, 0.0,
        network_time::NetworkTimeTracker::FETCHES_IN_BACKGROUND_ONLY);
  }

  SSLErrorHandlerDateInvalidTest(const SSLErrorHandlerDateInvalidTest&) =
      delete;
  SSLErrorHandlerDateInvalidTest& operator=(
      const SSLErrorHandlerDateInvalidTest&) = delete;

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    SSLErrorHandler::ResetConfigForTesting();

    base::RunLoop run_loop;
    std::unique_ptr<network::PendingSharedURLLoaderFactory>
        pending_url_loader_factory;
    content::GetIOThreadTaskRunner({})->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(CreateURLLoaderFactory, &pending_url_loader_factory),
        run_loop.QuitClosure());
    run_loop.Run();

    shared_url_loader_factory_ = network::SharedURLLoaderFactory::Create(
        std::move(pending_url_loader_factory));

    tracker_ = std::make_unique<network_time::NetworkTimeTracker>(
        std::unique_ptr<base::Clock>(clock_),
        std::unique_ptr<base::TickClock>(tick_clock_), &pref_service_,
        shared_url_loader_factory_, std::nullopt);
    // Do this to be sure that |is_null| returns false.
    clock_->Advance(base::Days(111));
    tick_clock_->Advance(base::Days(222));

    SSLErrorHandler::SetInterstitialDelayForTesting(base::TimeDelta());
    ssl_info_.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
    ssl_info_.cert_status = net::CERT_STATUS_DATE_INVALID;

    delegate_ = new TestSSLErrorHandlerDelegate(web_contents(), ssl_info_);
    error_handler_ = std::make_unique<TestSSLErrorHandler>(
        std::unique_ptr<SSLErrorHandler::Delegate>(delegate_), web_contents(),
        net::MapCertStatusToNetError(ssl_info_.cert_status), ssl_info_,
        tracker_.get(), GURL() /*request_url*/,
        /*captive_portal_service=*/nullptr);

    // Fix flakiness in case system time is off and triggers a bad clock
    // interstitial. https://crbug.com/666821#c50
    ssl_errors::SetBuildTimeForTesting(base::Time::Now());
  }

  void TearDown() override {
    // Release the reference on TestSharedURLLoaderFactory before the test
    // thread bundle flushes the IO thread so that it's destructed.
    shared_url_loader_factory_ = nullptr;

    if (error_handler()) {
      EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
      error_handler_.reset(nullptr);
    }
    SSLErrorHandler::ResetConfigForTesting();

    // content::RenderViewHostTestHarness::TearDown() simulates shutdown and as
    // such destroys parts of the task environment required in these
    // destructors.
    test_server_.reset();
    tracker_.reset();

    content::RenderViewHostTestHarness::TearDown();
  }

  TestSSLErrorHandler* error_handler() { return error_handler_.get(); }
  TestSSLErrorHandlerDelegate* delegate() { return delegate_; }

  network_time::FieldTrialTest* field_trial_test() {
    return field_trial_test_.get();
  }

  network_time::NetworkTimeTracker* tracker() { return tracker_.get(); }

  net::EmbeddedTestServer* test_server() { return test_server_.get(); }

  void ClearErrorHandler() { error_handler_.reset(nullptr); }

 private:
  static void CreateURLLoaderFactory(
      std::unique_ptr<network::PendingSharedURLLoaderFactory>*
          pending_url_loader_factory) {
    scoped_refptr<network::TestSharedURLLoaderFactory> factory =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>();
    // Holds a reference to |factory|.
    *pending_url_loader_factory = factory->Clone();
  }

  net::SSLInfo ssl_info_;
  std::unique_ptr<TestSSLErrorHandler> error_handler_;
  raw_ptr<TestSSLErrorHandlerDelegate, DanglingUntriaged> delegate_;

  std::unique_ptr<network_time::FieldTrialTest> field_trial_test_;
  raw_ptr<base::SimpleTestClock, DanglingUntriaged> clock_;
  raw_ptr<base::SimpleTestTickClock, DanglingUntriaged> tick_clock_;
  TestingPrefServiceSimple pref_service_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<network_time::NetworkTimeTracker> tracker_;
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
};

#if BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnTimerExpired) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());

  delegate()->ClearSeenOperations();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowCustomInterstitialOnCaptivePortalResult) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
  // Fake a captive portal result.
  delegate()->ClearSeenOperations();

  captive_portal::CaptivePortalService::Results results;
  results.previous_result = captive_portal::RESULT_INTERNET_CONNECTED;
  results.result = captive_portal::RESULT_BEHIND_CAPTIVE_PORTAL;

  error_handler()->Observe(results);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_TRUE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnNoCaptivePortalResult) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());
  // Fake a "connected to internet" result for the captive portal check.
  // This should immediately trigger an SSL interstitial without waiting for
  // the timer to expire.
  delegate()->ClearSeenOperations();

  captive_portal::CaptivePortalService::Results results;
  results.previous_result = captive_portal::RESULT_INTERNET_CONNECTED;
  results.result = captive_portal::RESULT_INTERNET_CONNECTED;

  error_handler()->Observe(results);
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldNotCheckSuggestedUrlIfNoSuggestedUrl) {
  base::HistogramTester histograms;
  error_handler()->StartHandlingError();

  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->suggested_url_checked());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldNotCheckCaptivePortalIfSuggestedUrlExists) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->suggested_url_checked());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());

  // Note that the suggested URL check is never completed, so there is no entry
  // for WWW_MISMATCH_URL_AVAILABLE or WWW_MISMATCH_URL_NOT_AVAILABLE.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldNotHandleNameMismatchOnNonOverridableError) {
  base::HistogramTester histograms;
  delegate()->set_non_overridable_error();
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_FALSE(delegate()->suggested_url_checked());
  EXPECT_TRUE(delegate()->captive_portal_checked());
  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 1);
}

#else  // #if !BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnCaptivePortalDetectionDisabled) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

#endif  // BUILDFLAG(ENABLE_CAPTIVE_PORTAL_DETECTION)

// Test that a captive portal interstitial is shown if the OS reports a portal.
TEST_F(SSLErrorHandlerNameMismatchTest, OSReportsCaptivePortal) {
  base::HistogramTester histograms;
  delegate()->set_os_reports_captive_portal();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_TRUE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_CAPTIVE_PORTAL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::OS_REPORTS_CAPTIVE_PORTAL, 1);
}

class SSLErrorHandlerNameMismatchCaptivePortalInterstitialDisabledTest
    : public SSLErrorHandlerNameMismatchTest {
 public:
  SSLErrorHandlerNameMismatchCaptivePortalInterstitialDisabledTest() {
    scoped_feature_list_.InitAndDisableFeature(kCaptivePortalInterstitial);
  }

 private:
  // This should only be accessed from a test's constructor, to avoid tsan data
  // races with threads kicked off by RenderViewHostTestHarness::SetUp().
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that a captive portal interstitial isn't shown if the OS reports a
// portal but CaptivePortalInterstitial feature is disabled.
TEST_F(SSLErrorHandlerNameMismatchCaptivePortalInterstitialDisabledTest,
       OSReportsCaptivePortal_FeatureDisabled) {
  base::HistogramTester histograms;
  delegate()->set_os_reports_captive_portal();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  error_handler()->StartHandlingError();
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->captive_portal_checked());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->captive_portal_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::OS_REPORTS_CAPTIVE_PORTAL, 0);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnTimerExpiredWhenSuggestedUrlExists) {
  base::HistogramTester histograms;
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->suggested_url_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());

  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());

  // Note that the suggested URL check is never completed, so there is no entry
  // for WWW_MISMATCH_URL_AVAILABLE or WWW_MISMATCH_URL_NOT_AVAILABLE.
  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldRedirectOnSuggestedUrlCheckResult) {
  base::HistogramTester histograms;
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->suggested_url_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());
  // Fake a valid suggested URL check result.
  // The URL returned by |SuggestedUrlCheckResult| can be different from
  // |suggested_url|, if there is a redirect.
  delegate()->SendSuggestedUrlCheckResult(
      CommonNameMismatchHandler::SuggestedUrlCheckResult::
          SUGGESTED_URL_AVAILABLE,
      GURL("https://random.example.com"));

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_TRUE(delegate()->redirected_to_suggested_url());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 3);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_URL_AVAILABLE, 1);
}

// No suggestions should be requested if certificate lacks a SubjectAltName.
TEST_F(SSLErrorHandlerNameMismatchNoSANTest,
       SSLCommonNameMismatchHandlingRequiresSubjectAltName) {
  base::HistogramTester histograms;
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_FALSE(delegate()->suggested_url_checked());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 0);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

TEST_F(SSLErrorHandlerNameMismatchTest,
       ShouldShowSSLInterstitialOnInvalidUrlCheckResult) {
  base::HistogramTester histograms;
  delegate()->set_suggested_url_exists();
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->suggested_url_checked());
  EXPECT_FALSE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());
  // Fake an Invalid Suggested URL Check result.
  delegate()->SendSuggestedUrlCheckResult(
      CommonNameMismatchHandler::SuggestedUrlCheckResult::
          SUGGESTED_URL_NOT_AVAILABLE,
      GURL());

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->redirected_to_suggested_url());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 4);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_FOUND_IN_SAN, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::WWW_MISMATCH_URL_NOT_AVAILABLE,
                               1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
}

// Flakily fails on linux_chromium_tsan_rel_ng. http://crbug.com/989128
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(THREAD_SANITIZER)
#define MAYBE_TimeQueryStarted DISABLED_TimeQueryStarted
#else
#define MAYBE_TimeQueryStarted TimeQueryStarted
#endif
TEST_F(SSLErrorHandlerDateInvalidTest, MAYBE_TimeQueryStarted) {
  base::HistogramTester histograms;
  base::Time network_time;
  base::TimeDelta uncertainty;
  SSLErrorHandler::SetInterstitialDelayForTesting(base::Hours(1));
  EXPECT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker()->GetNetworkTime(&network_time, &uncertainty));

  // Enable network time queries and handle the error. A bad clock interstitial
  // should be shown.
  test_server()->RegisterRequestHandler(
      base::BindRepeating(&network_time::GoodTimeResponseHandler));
  EXPECT_TRUE(test_server()->Start());
  tracker()->SetTimeServerURLForTesting(test_server()->GetURL("/"));
  field_trial_test()->SetFeatureParams(
      true, 0.0, network_time::NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY);
  error_handler()->StartHandlingError();

  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  tracker()->WaitForFetchForTesting(123123123);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate()->bad_clock_interstitial_shown());
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
}

// Tests that an SSL interstitial is shown if the accuracy of the system
// clock can't be determined because network time is unavailable.

// Flakily fails on linux_chromium_tsan_rel_ng. http://crbug.com/989225
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(THREAD_SANITIZER)
#define MAYBE_NoTimeQueries DISABLED_NoTimeQueries
#else
#define MAYBE_NoTimeQueries NoTimeQueries
#endif
TEST_F(SSLErrorHandlerDateInvalidTest, MAYBE_NoTimeQueries) {
  base::HistogramTester histograms;
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker()->GetNetworkTime(&network_time, &uncertainty));

  // Handle the error without enabling time queries. A bad clock interstitial
  // should not be shown.
  error_handler()->StartHandlingError();

  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());
  EXPECT_FALSE(delegate()->bad_clock_interstitial_shown());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
}

// Tests that an SSL interstitial is shown if determing the accuracy of
// the system clock times out (e.g. because a network time query hangs).

// Flakily fails on linux_chromium_tsan_rel_ng. http://crbug.com/989289
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && defined(THREAD_SANITIZER)
#define MAYBE_TimeQueryHangs DISABLED_TimeQueryHangs
#else
#define MAYBE_TimeQueryHangs TimeQueryHangs
#endif
TEST_F(SSLErrorHandlerDateInvalidTest, MAYBE_TimeQueryHangs) {
  base::HistogramTester histograms;
  base::Time network_time;
  base::TimeDelta uncertainty;
  EXPECT_EQ(network_time::NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT,
            tracker()->GetNetworkTime(&network_time, &uncertainty));

  // Enable network time queries and handle the error. Because the
  // network time cannot be determined before the timer elapses, an SSL
  // interstitial should be shown.
  base::RunLoop wait_for_time_query_loop;
  test_server()->RegisterRequestHandler(base::BindRepeating(
      &WaitForRequest, wait_for_time_query_loop.QuitClosure()));
  EXPECT_TRUE(test_server()->Start());
  tracker()->SetTimeServerURLForTesting(test_server()->GetURL("/"));
  field_trial_test()->SetFeatureParams(
      true, 0.0, network_time::NetworkTimeTracker::FETCHES_ON_DEMAND_ONLY);
  error_handler()->StartHandlingError();
  EXPECT_TRUE(error_handler()->IsTimerRunningForTesting());
  wait_for_time_query_loop.Run();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(delegate()->bad_clock_interstitial_shown());
  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(error_handler()->IsTimerRunningForTesting());

  // Clear the error handler to test that, when the request completes,
  // it doesn't try to call a callback on a deleted SSLErrorHandler.
  ClearErrorHandler();

  // Shut down the server to cancel the pending request.
  ASSERT_TRUE(test_server()->ShutdownAndWaitUntilComplete());
}

// Tests that if a certificate matches the issuer common name regex of a MITM
// software entry but not the issuer organization name a MITM software
// interstitial will not be displayed.
TEST_F(SSLErrorAssistantProtoMITMSoftwareEnabledTest,
       MITMSoftware_CertificateDoesNotMatchOrganizationName_NoInterstitial) {
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  chrome_browser_ssl::MITMSoftware* filter = config_proto->add_mitm_software();
  filter->set_name("Misconfigured Firewall");
  filter->set_issuer_common_name_regex("Misconfigured Firewall_[A-Z0-9]+");
  filter->set_issuer_organization_regex("Non-Matching Organization Name");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  TestNoMITMSoftwareInterstitial();
}

// Tests that if a certificate matches the issuer organization name regex of a
// MITM software entry but not the issuer common name a MITM software
// interstitial will not be displayed.
TEST_F(SSLErrorAssistantProtoMITMSoftwareEnabledTest,
       MITMSoftware_CertificateDoesNotMatchCommonName_NoInterstitial) {
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);

  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  chrome_browser_ssl::MITMSoftware* filter = config_proto->add_mitm_software();
  filter->set_name("Misconfigured Firewall");
  filter->set_issuer_common_name_regex("Non-Matching Issuer Common Name");
  filter->set_issuer_organization_regex("Misconfigured Firewall");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  TestNoMITMSoftwareInterstitial();
}

// Tests that a certificate with no organization name or common name will not
// trigger a MITM software interstitial.
TEST_F(SSLErrorAssistantProtoMITMSoftwareEnabledTest,
       MITMSoftware_CertificateWithNoOrganizationOrCommonName_NoInterstitial) {
  ResetErrorHandlerFromString(kCertWithoutOrganizationOrCommonName,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  InitMITMSoftwareList();
  TestNoMITMSoftwareInterstitial();
}

// Tests that when everything else is in order, a matching MITM software
// certificate will trigger the MITM software interstitial.
TEST_F(SSLErrorAssistantProtoMITMSoftwareEnabledTest,
       MITMSoftware_CertificateMatchesCommonNameAndOrganizationName) {
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  InitMITMSoftwareList();
  TestMITMSoftwareInterstitial();
}

// Tests that a known MITM software entry in the SSL error assistant proto that
// has a common name regex but not an organization name regex can still trigger
// a MITM software interstitial.
TEST_F(SSLErrorAssistantProtoMITMSoftwareEnabledTest,
       MITMSoftware_CertificateMatchesCommonName) {
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  // Register a MITM Software entry in the SSL error assistant proto that has a
  // common name regex but not an organization name regex.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  chrome_browser_ssl::MITMSoftware* filter = config_proto->add_mitm_software();
  filter->set_name("Misconfigured Firewall");
  filter->set_issuer_common_name_regex("Misconfigured Firewall_[A-Z0-9]+");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  TestMITMSoftwareInterstitial();
}

// Tests that a known MITM software entry in the SSL error assistant proto that
// has an organization name regex but not a common name name regex can still
// trigger a MITM software interstitial.
TEST_F(SSLErrorAssistantProtoMITMSoftwareEnabledTest,
       MITMSoftware_CertificateMatchesOrganizationName) {
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  // Register a MITM Software entry in the SSL error assistant proto that has an
  // organization name regex, but not a common name regex.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  chrome_browser_ssl::MITMSoftware* filter = config_proto->add_mitm_software();
  filter->set_name("Misconfigured Firewall");
  filter->set_issuer_organization_regex("Misconfigured Firewall");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  TestMITMSoftwareInterstitial();
}

// Tests that only a full regex match will trigger the MITM software
// interstitial. For example, a common name regex "Match" should not trigger the
// MITM software interstitial on a certificate that's common name is
// "Full Match".
TEST_F(SSLErrorAssistantProtoMITMSoftwareEnabledTest,
       MITMSoftware_PartialRegexMatch_NoInterstitial) {
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  // Register a MITM software entry with common name and organization name
  // regexes that will match part of each the certificate's common name and
  // organization name fields but not the entire field.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(kLargeVersionId);

  chrome_browser_ssl::MITMSoftware* filter = config_proto->add_mitm_software();
  filter->set_name("Misconfigured Firewall");
  filter->set_issuer_common_name_regex("Misconfigured");
  filter->set_issuer_organization_regex("Misconfigured");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));
  TestNoMITMSoftwareInterstitial();
}

// Tests that a MITM software interstitial is not triggered when neither the
// common name or the organization name match.
TEST_F(SSLErrorAssistantProtoMITMSoftwareEnabledTest,
       MITMSoftware_NonMatchingCertificate_NoInterstitial) {
  ResetErrorHandlerFromFile(kOkayCertName, net::CERT_STATUS_AUTHORITY_INVALID);
  InitMITMSoftwareList();
  TestNoMITMSoftwareInterstitial();
}

// Tests that the MITM software interstitial is not triggered when the feature
// is disabled by Finch.
TEST_F(SSLErrorAssistantProtoMITMSoftwareDisabledTest,
       MITMSoftware_FeatureDisabled) {
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  InitMITMSoftwareList();
  TestNoMITMSoftwareInterstitial();
}

// Tests that the MITM software interstitial is not triggered when an error
// other than net::CERT_STATUS_AUTHORITY_INVALID is thrown.
TEST_F(SSLErrorAssistantProtoMITMSoftwareEnabledTest,
       MITMSoftware_WrongError_NoInterstitial) {
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_COMMON_NAME_INVALID);
  InitMITMSoftwareList();
  TestNoMITMSoftwareInterstitial();
}

// Tests that the MITM software interstitial is not triggered when more than one
// error is thrown.
TEST_F(SSLErrorAssistantProtoMITMSoftwareEnabledTest,
       MITMSoftware_TwoErrors_NoInterstitial) {
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID |
                                  net::CERT_STATUS_COMMON_NAME_INVALID);
  InitMITMSoftwareList();
  TestNoMITMSoftwareInterstitial();
}

// Tests that the MITM software interstitial is not triggered if the error
// thrown is overridable.
TEST_F(SSLErrorAssistantProtoMITMSoftwareEnabledTest,
       MITMSoftware_Overridable_NoInterstitial) {
  base::HistogramTester histograms;

  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);
  InitMITMSoftwareList();
  error_handler()->StartHandlingError();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(delegate()->ssl_interstitial_shown());
  EXPECT_FALSE(delegate()->mitm_software_interstitial_shown());
  EXPECT_FALSE(delegate()->suggested_url_checked());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_NONOVERRIDABLE, 0);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_SSL_INTERSTITIAL_OVERRIDABLE, 1);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::SHOW_MITM_SOFTWARE_INTERSTITIAL,
                               0);
}

TEST_F(SSLErrorAssistantProtoMITMSoftwareEnabledTest,
       MITMSoftware_IgnoreDynamicUpdateWithSmallVersionId) {
  ResetErrorHandlerFromString(kMisconfiguredFirewallCert,
                              net::CERT_STATUS_AUTHORITY_INVALID);

  // Register a MITM Software entry in the SSL error assistant proto that has a
  // common name regex but not an organization name regex. This should normally
  // trigger a MITM software interstitial, but the version_id is zero which is
  // less than the version_id of the local resource bundle, so the dynamic
  // update will be ignored.
  auto config_proto =
      std::make_unique<chrome_browser_ssl::SSLErrorAssistantConfig>();
  config_proto->set_version_id(0u);

  chrome_browser_ssl::MITMSoftware* filter = config_proto->add_mitm_software();
  filter->set_name("Misconfigured Firewall");
  filter->set_issuer_common_name_regex("Misconfigured Firewall_[A-Z0-9]+");
  SSLErrorHandler::SetErrorAssistantProto(std::move(config_proto));

  TestNoMITMSoftwareInterstitial();
}

using SSLErrorHandlerTest = content::RenderViewHostTestHarness;

// Test that a blocked interception interstitial is shown. It would be nicer to
// set the SSLInfo properly so that the cert is blocked at net level rather than
// because of set_has_blocked_interception(), but that code path is already
// executed in net unit tests and SSL browser tests. This test mainly checks
// histogram accuracy.
TEST_F(SSLErrorHandlerTest, BlockedInterceptionInterstitial) {
  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), kOkayCertName);
  ssl_info.cert_status = net::CERT_STATUS_COMMON_NAME_INVALID;
  ssl_info.public_key_hashes.push_back(net::HashValue(kCertPublicKeyHashValue));

  std::unique_ptr<TestSSLErrorHandlerDelegate> delegate(
      new TestSSLErrorHandlerDelegate(web_contents(), ssl_info));

  TestSSLErrorHandlerDelegate* delegate_ptr = delegate.get();
  TestSSLErrorHandler error_handler(
      std::move(delegate), web_contents(),
      net::MapCertStatusToNetError(ssl_info.cert_status), ssl_info,
      /*network_time_tracker=*/nullptr, GURL() /*request_url*/,
      /*captive_portal_service=*/nullptr);

  base::HistogramTester histograms;
  delegate_ptr->set_has_blocked_interception();

  EXPECT_FALSE(error_handler.IsTimerRunningForTesting());
  error_handler.StartHandlingError();
  EXPECT_FALSE(error_handler.IsTimerRunningForTesting());
  EXPECT_FALSE(delegate_ptr->captive_portal_checked());
  EXPECT_FALSE(delegate_ptr->ssl_interstitial_shown());
  EXPECT_FALSE(delegate_ptr->captive_portal_interstitial_shown());
  EXPECT_TRUE(delegate_ptr->blocked_interception_interstitial_shown());

  histograms.ExpectTotalCount(SSLErrorHandler::GetHistogramNameForTesting(), 2);
  histograms.ExpectBucketCount(SSLErrorHandler::GetHistogramNameForTesting(),
                               SSLErrorHandler::HANDLE_ALL, 1);
  histograms.ExpectBucketCount(
      SSLErrorHandler::GetHistogramNameForTesting(),
      SSLErrorHandler::SHOW_BLOCKED_INTERCEPTION_INTERSTITIAL, 1);
}

// Tests that non-primary main frame navigations should not affect
// SSLErrorHandler.
TEST_F(SSLErrorHandlerTest, NonPrimaryMainframeShouldNotAffectSSLErrorHandler) {
  net::SSLInfo ssl_info;
  ssl_info.cert =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), kOkayCertName);
  ssl_info.cert_status = net::CERT_STATUS_AUTHORITY_INVALID;
  ssl_info.public_key_hashes.push_back(net::HashValue(kCertPublicKeyHashValue));

  std::unique_ptr<TestSSLErrorHandlerDelegate> delegate(
      new TestSSLErrorHandlerDelegate(web_contents(), ssl_info));

  auto error_handler = std::make_unique<TestSSLErrorHandler>(
      std::move(delegate), web_contents(),
      net::MapCertStatusToNetError(ssl_info.cert_status), ssl_info,
      /*network_time_tracker=*/nullptr, /*request_url=*/GURL(),
      /*captive_portal_service=*/nullptr);

  auto* error_handler_ptr = error_handler.get();
  web_contents()->SetUserData(SSLErrorHandler::UserDataKey(),
                              std::move(error_handler));

  std::unique_ptr<content::MockNavigationHandle> handle =
      std::make_unique<content::MockNavigationHandle>(GURL(), main_rfh());
  handle->set_is_in_primary_main_frame(false);
  error_handler_ptr->DidStartNavigation(handle.get());
  // Make sure that the |SSLErrorHandler| is not deleted.
  EXPECT_TRUE(SSLErrorHandler::FromWebContents(web_contents()));

  handle->set_is_in_primary_main_frame(true);
  error_handler_ptr->DidStartNavigation(handle.get());
  // Make sure that the |SSLErrorHandler| is deleted.
  EXPECT_FALSE(SSLErrorHandler::FromWebContents(web_contents()));
}
