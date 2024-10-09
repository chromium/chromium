// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/core/security_state.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_certificate_data.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace security_state {

namespace {

const char kHttpsUrl[] = "https://foo.test/";
const char kHttpUrl[] = "http://foo.test/";
const char kLocalhostUrl[] = "http://localhost";
const char kFileOrigin[] = "file://example_file";
const char kWssUrl[] = "wss://foo.test/";
const char kDataUrl[] = "data:text/html,<html>test</html>";

// This list doesn't include data: URL, as data: URLs will be explicitly marked
// as not secure.
const char* const kPseudoUrls[] = {
    "blob:http://test/some-guid", "filesystem:http://test/some-guid",
};

class TestSecurityStateHelper {
 public:
  TestSecurityStateHelper()
      : url_(kHttpsUrl),
        cert_(net::ImportCertFromFile(net::GetTestCertsDirectory(),
                                      "sha1_2016.pem")),
        connection_status_(net::SSL_CONNECTION_VERSION_TLS1_2
                           << net::SSL_CONNECTION_VERSION_SHIFT),
        cert_status_(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT),
        displayed_mixed_content_(false),
        contained_mixed_form_(false),
        ran_mixed_content_(false),
        malicious_content_status_(MALICIOUS_CONTENT_STATUS_NONE),
        is_error_page_(false),
        is_view_source_(false),
        has_policy_certificate_(false),
        safety_tip_info_({security_state::SafetyTipStatus::kUnknown, GURL()}),
        is_https_only_mode_upgraded_(false) {}
  virtual ~TestSecurityStateHelper() = default;

  void SetCertificate(scoped_refptr<net::X509Certificate> cert) {
    cert_ = std::move(cert);
  }
  void set_connection_status(int connection_status) {
    connection_status_ = connection_status;
  }
  void SetCipherSuite(uint16_t ciphersuite) {
    net::SSLConnectionStatusSetCipherSuite(ciphersuite, &connection_status_);
  }
  void AddCertStatus(net::CertStatus cert_status) {
    cert_status_ |= cert_status;
  }
  void set_cert_status(net::CertStatus cert_status) {
    cert_status_ = cert_status;
  }
  void set_displayed_mixed_content(bool displayed_mixed_content) {
    displayed_mixed_content_ = displayed_mixed_content;
  }
  void set_contained_mixed_form(bool contained_mixed_form) {
    contained_mixed_form_ = contained_mixed_form;
  }
  void set_ran_mixed_content(bool ran_mixed_content) {
    ran_mixed_content_ = ran_mixed_content;
  }
  void set_malicious_content_status(
      MaliciousContentStatus malicious_content_status) {
    malicious_content_status_ = malicious_content_status;
  }

  void set_is_error_page(bool is_error_page) { is_error_page_ = is_error_page; }

  void set_is_view_source(bool is_view_source) {
    is_view_source_ = is_view_source;
  }

  void set_has_policy_certificate(bool has_policy_cert) {
    has_policy_certificate_ = has_policy_cert;
  }
  void SetUrl(const GURL& url) { url_ = url; }

  void set_safety_tip_status(
      security_state::SafetyTipStatus safety_tip_status) {
    safety_tip_info_.status = safety_tip_status;
  }

  void set_is_https_only_mode_upgraded(bool is_https_only_mode_upgraded) {
    is_https_only_mode_upgraded_ = is_https_only_mode_upgraded;
  }

  std::unique_ptr<VisibleSecurityState> GetVisibleSecurityState() const {
    auto state = std::make_unique<VisibleSecurityState>();
    state->connection_info_initialized = true;
    state->url = url_;
    state->certificate = cert_;
    state->cert_status = cert_status_;
    state->connection_status = connection_status_;
    state->displayed_mixed_content = displayed_mixed_content_;
    state->contained_mixed_form = contained_mixed_form_;
    state->ran_mixed_content = ran_mixed_content_;
    state->malicious_content_status = malicious_content_status_;
    state->is_error_page = is_error_page_;
    state->is_view_source = is_view_source_;
    state->safety_tip_info = safety_tip_info_;
    state->is_https_only_mode_upgraded = is_https_only_mode_upgraded_;
    return state;
  }

  security_state::SecurityLevel GetSecurityLevel() const {
    return security_state::GetSecurityLevel(*GetVisibleSecurityState(),
                                            has_policy_certificate_);
  }

  bool HasMajorCertificateError() const {
    return security_state::HasMajorCertificateError(*GetVisibleSecurityState());
  }

 private:
  GURL url_;
  scoped_refptr<net::X509Certificate> cert_;
  int connection_status_;
  net::CertStatus cert_status_;
  bool displayed_mixed_content_;
  bool contained_mixed_form_;
  bool ran_mixed_content_;
  MaliciousContentStatus malicious_content_status_;
  bool is_error_page_;
  bool is_view_source_;
  bool has_policy_certificate_;
  security_state::SafetyTipInfo safety_tip_info_;
  bool is_https_only_mode_upgraded_;
};

}  // namespace

// Tests that SHA1-signed certificates, when not allowed by policy, downgrade
// the security state of the page to DANGEROUS.
TEST(SecurityStateTest, SHA1Blocked) {
  TestSecurityStateHelper helper;
  helper.AddCertStatus(net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);
  helper.AddCertStatus(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT);
  EXPECT_TRUE(security_state::IsSHA1InChain(*helper.GetVisibleSecurityState()));
  EXPECT_EQ(DANGEROUS, helper.GetSecurityLevel());

  // Ensure that policy-installed certificates do not interfere.
  helper.set_has_policy_certificate(true);
  EXPECT_TRUE(security_state::IsSHA1InChain(*helper.GetVisibleSecurityState()));
  EXPECT_EQ(DANGEROUS, helper.GetSecurityLevel());
}

// Tests that SHA1-signed certificates, when allowed by policy, downgrade the
// security state of the page to NONE.
TEST(SecurityStateTest, SHA1Warning) {
  TestSecurityStateHelper helper;
  EXPECT_TRUE(security_state::IsSHA1InChain(*helper.GetVisibleSecurityState()));
  EXPECT_EQ(NONE, helper.GetSecurityLevel());

  // Ensure that policy-installed certificates do not interfere.
  helper.set_has_policy_certificate(true);
  EXPECT_TRUE(security_state::IsSHA1InChain(*helper.GetVisibleSecurityState()));
  EXPECT_EQ(NONE, helper.GetSecurityLevel());
}

// Tests that SHA1-signed certificates, when allowed by policy, don't interfere
// with the handling of mixed content.
TEST(SecurityStateTest, SHA1WarningMixedContent) {
  TestSecurityStateHelper helper;
  helper.set_displayed_mixed_content(true);
  EXPECT_TRUE(security_state::IsSHA1InChain(*helper.GetVisibleSecurityState()));
  EXPECT_EQ(NONE, helper.GetSecurityLevel());

  helper.set_displayed_mixed_content(false);
  helper.set_ran_mixed_content(true);
  EXPECT_TRUE(security_state::IsSHA1InChain(*helper.GetVisibleSecurityState()));
  EXPECT_EQ(DANGEROUS, helper.GetSecurityLevel());
}

// Tests that SHA1-signed certificates, when allowed by policy,
// don't interfere with the handling of major cert errors.
TEST(SecurityStateTest, SHA1WarningBrokenHTTPS) {
  TestSecurityStateHelper helper;
  helper.AddCertStatus(net::CERT_STATUS_DATE_INVALID);
  EXPECT_TRUE(security_state::IsSHA1InChain(*helper.GetVisibleSecurityState()));
  EXPECT_EQ(DANGEROUS, helper.GetSecurityLevel());
}

// Tests that the malware/phishing status overrides valid HTTPS.
TEST(SecurityStateTest, MalwareOverride) {
  TestSecurityStateHelper helper;
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  helper.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  helper.SetCipherSuite(ciphersuite);

  helper.set_malicious_content_status(MALICIOUS_CONTENT_STATUS_MALWARE);

  EXPECT_EQ(DANGEROUS, helper.GetSecurityLevel());
}

// Tests that the malware/phishing status is set, even if other connection info
// is not available.
TEST(SecurityStateTest, MalwareWithoutConnectionState) {
  TestSecurityStateHelper helper;
  helper.set_malicious_content_status(
      MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING);
  EXPECT_EQ(DANGEROUS, helper.GetSecurityLevel());
}

// Tests that pseudo URLs always cause an WARNING to be shown.
TEST(SecurityStateTest, AlwaysWarnOnDataUrls) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kDataUrl));
  EXPECT_EQ(WARNING, helper.GetSecurityLevel());
}

// Tests that the security level is downgraded to WARNING on
// pseudo URLs.
TEST(SecurityStateTest, WarningOnPseudoUrls) {
  for (const char* const url : kPseudoUrls) {
    TestSecurityStateHelper helper;
    helper.SetUrl(GURL(url));
    EXPECT_EQ(WARNING, helper.GetSecurityLevel());
  }
}

// Tests that if |is_view_source| is set, NONE is returned for a secure site.
TEST(SecurityStateTest, ViewSourceRemovesSecure) {
  TestSecurityStateHelper helper;
  helper.set_cert_status(0);
  EXPECT_EQ(SECURE, helper.GetSecurityLevel());
  helper.set_is_view_source(true);
  EXPECT_EQ(NONE, helper.GetSecurityLevel());
}

// Tests that if |is_view_source| is set, DANGEROUS is still returned for a site
// flagged by SafeBrowsing.
TEST(SecurityStateTest, ViewSourceKeepsWarning) {
  TestSecurityStateHelper helper;
  helper.set_malicious_content_status(
      MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING);
  helper.set_is_view_source(true);
  EXPECT_EQ(DANGEROUS, helper.GetSecurityLevel());
}

// Tests that a mixed form is reflected in the security level.
TEST(SecurityStateTest, MixedForm) {
  TestSecurityStateHelper helper;
  helper.set_contained_mixed_form(true);

  // Verify that a mixed form downgrades the security level.
  EXPECT_EQ(NONE, helper.GetSecurityLevel());

  // Ensure that active mixed content trumps the mixed form warning.
  helper.set_ran_mixed_content(true);
  EXPECT_EQ(DANGEROUS, helper.GetSecurityLevel());
}

// Tests that policy-installed-certificates do not interfere with mixed content
// notifications.
TEST(SecurityStateTest, MixedContentWithPolicyCertificate) {
  TestSecurityStateHelper helper;

  helper.set_has_policy_certificate(true);
  helper.set_cert_status(0);

  // Verify that if no mixed content is present, the policy-installed
  // certificate is recorded.
  EXPECT_EQ(SECURE_WITH_POLICY_INSTALLED_CERT, helper.GetSecurityLevel());

  // Verify that a mixed form downgrades the security level.
  helper.set_contained_mixed_form(true);
  EXPECT_EQ(NONE, helper.GetSecurityLevel());

  // Verify that passive mixed content downgrades the security level.
  helper.set_contained_mixed_form(false);
  helper.set_displayed_mixed_content(true);
  SecurityLevel expected_passive_level = WARNING;
  EXPECT_EQ(expected_passive_level, helper.GetSecurityLevel());

  // Ensure that active mixed content downgrades the security level.
  helper.set_contained_mixed_form(false);
  helper.set_displayed_mixed_content(false);
  helper.set_ran_mixed_content(true);
  EXPECT_EQ(DANGEROUS, helper.GetSecurityLevel());
}

// Tests that HTTP URLs cause a WARNING security level.
TEST(SecurityStateTest, WarningOnHttp) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));

  EXPECT_EQ(WARNING, helper.GetSecurityLevel());
}

// Tests that |safety_tip_status| effects security level appropriately.
TEST(SecurityStateTest, SafetyTipSometimesRemovesSecure) {
  using security_state::SafetyTipStatus;

  struct SafetyTipCase {
    SafetyTipStatus safety_tip_status;
    security_state::SecurityLevel expected_level;
  };

  const SafetyTipCase kTestCases[] = {
      {SafetyTipStatus::kUnknown, SECURE},
      {SafetyTipStatus::kNone, SECURE},
      {SafetyTipStatus::kLookalike, SECURE},
  };

  for (auto testcase : kTestCases) {
    TestSecurityStateHelper helper;
    helper.set_cert_status(0);
    EXPECT_EQ(SECURE, helper.GetSecurityLevel());
    helper.set_safety_tip_status(testcase.safety_tip_status);
    EXPECT_EQ(testcase.expected_level, helper.GetSecurityLevel());
  }
}

// Tests IsSchemeCryptographic function.
TEST(SecurityStateTest, CryptographicSchemeUrl) {
  // HTTPS is a cryptographic scheme.
  EXPECT_TRUE(IsSchemeCryptographic(GURL(kHttpsUrl)));
  // WSS is a cryptographic scheme.
  EXPECT_TRUE(IsSchemeCryptographic(GURL(kWssUrl)));
  // HTTP is not a cryptographic scheme.
  EXPECT_FALSE(IsSchemeCryptographic(GURL(kHttpUrl)));
  // Return true only for valid |url|
  EXPECT_FALSE(IsSchemeCryptographic(GURL("https://")));
}

// Tests IsOriginLocalhostOrFile function.
TEST(SecurityStateTest, LocalhostOrFileUrl) {
  EXPECT_TRUE(IsOriginLocalhostOrFile(GURL(kLocalhostUrl)));
  EXPECT_TRUE(IsOriginLocalhostOrFile(GURL(kFileOrigin)));
  EXPECT_FALSE(IsOriginLocalhostOrFile(GURL(kHttpsUrl)));
}

// Tests IsSslCertificateValid function.
TEST(SecurityStateTest, SslCertificateValid) {
  EXPECT_TRUE(IsSslCertificateValid(SecurityLevel::SECURE));
  EXPECT_TRUE(
      IsSslCertificateValid(SecurityLevel::SECURE_WITH_POLICY_INSTALLED_CERT));

  EXPECT_FALSE(IsSslCertificateValid(SecurityLevel::NONE));
  EXPECT_FALSE(IsSslCertificateValid(SecurityLevel::DANGEROUS));
  EXPECT_FALSE(IsSslCertificateValid(SecurityLevel::WARNING));
}

// Tests that WARNING is not set for error pages.
TEST(SecurityStateTest, ErrorPage) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL("http://nonexistent.test"));
  helper.set_is_error_page(true);
  EXPECT_EQ(SecurityLevel::NONE, helper.GetSecurityLevel());

  // Sanity-check that if it's not an error page, the security level is
  // downgraded.
  helper.set_is_error_page(false);
  EXPECT_EQ(SecurityLevel::WARNING, helper.GetSecurityLevel());
}

// Tests that the billing status is set, and it overrides valid HTTPS.
TEST(SecurityStateTest, BillingOverridesValidHTTPS) {
  TestSecurityStateHelper helper;
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  helper.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  helper.SetCipherSuite(ciphersuite);

  helper.set_malicious_content_status(MALICIOUS_CONTENT_STATUS_BILLING);

  EXPECT_EQ(DANGEROUS, helper.GetSecurityLevel());
}

// Tests that the billing status overrides HTTP warnings.
TEST(SecurityStateTest, BillingOverridesHTTPWarning) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));

  // Expect to see a warning for HTTP first.
  EXPECT_EQ(security_state::WARNING, helper.GetSecurityLevel());

  // Now mark the URL as matching the billing list.
  helper.set_malicious_content_status(MALICIOUS_CONTENT_STATUS_BILLING);
  // Expect to see a warning for billing now.
  EXPECT_EQ(DANGEROUS, helper.GetSecurityLevel());
}

// Tests that non-cryptographic schemes are handled as having no certificate
// errors.
TEST(SecurityStateTest, NonCryptoHasNoCertificateErrors) {
  TestSecurityStateHelper helper;
  helper.set_cert_status(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT |
                         net::CERT_STATUS_REVOKED);

  helper.SetUrl(GURL(kHttpUrl));
  EXPECT_FALSE(helper.HasMajorCertificateError());

  helper.SetUrl(GURL(kDataUrl));
  EXPECT_FALSE(helper.HasMajorCertificateError());
}

// Tests that cryptographic schemes without certificate errors are acceptable.
TEST(SecurityStateTest, CryptoWithNoCertificateErrors) {
  TestSecurityStateHelper helper;
  EXPECT_FALSE(helper.HasMajorCertificateError());

  helper.set_cert_status(0);
  EXPECT_FALSE(helper.HasMajorCertificateError());

  helper.SetCertificate(nullptr);
  EXPECT_FALSE(helper.HasMajorCertificateError());
}

// Tests that major certificate errors are detected.
TEST(SecurityStateTest, MajorCertificateErrors) {
  TestSecurityStateHelper helper;
  helper.set_cert_status(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT |
                         net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION);
  EXPECT_TRUE(helper.HasMajorCertificateError());

  helper.set_cert_status(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT |
                         net::CERT_STATUS_NO_REVOCATION_MECHANISM);
  EXPECT_TRUE(helper.HasMajorCertificateError());

  helper.set_cert_status(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT |
                         net::CERT_STATUS_REVOKED);
  EXPECT_TRUE(helper.HasMajorCertificateError());

  helper.set_cert_status(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT |
                         net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);
  EXPECT_TRUE(helper.HasMajorCertificateError());

  helper.set_cert_status(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT |
                         net::CERT_STATUS_PINNED_KEY_MISSING);
  EXPECT_TRUE(helper.HasMajorCertificateError());
}

// Tests that if a page was upgraded by HTTPS-Only Mode it takes precedence
// over net errors where connection info is not set.
TEST(SecurityStateTest, HttpsOnlyModeOverridesNetError) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL("https://nonexistent.test"));
  helper.set_is_error_page(true);
  helper.set_is_https_only_mode_upgraded(true);
  EXPECT_EQ(SecurityLevel::WARNING, helper.GetSecurityLevel());
}

// Tests that if a page was upgraded by HTTPS-Only Mode it takes precedence
// over the page having certificate errors.
TEST(SecurityStateTest, HttpsOnlyModeOverridesCertificateError) {
  TestSecurityStateHelper helper;
  helper.set_cert_status(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT |
                         net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION);
  EXPECT_TRUE(helper.HasMajorCertificateError());
  helper.set_is_error_page(true);
  helper.set_is_https_only_mode_upgraded(true);
  EXPECT_EQ(SecurityLevel::WARNING, helper.GetSecurityLevel());
}

// Tests that malicious content status takes precedence over HTTPS-Only Mode.
TEST(SecurityStateTest, MaliciousContentOverridesHttpsOnlyMode) {
  TestSecurityStateHelper helper;
  helper.set_malicious_content_status(
      MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING);
  helper.set_is_error_page(true);
  helper.set_is_https_only_mode_upgraded(true);
  EXPECT_EQ(DANGEROUS, helper.GetSecurityLevel());
}

}  // namespace security_state
