// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/core/security_state.h"

#include <stdint.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/insecure_input_event_data.h"
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

// This list doesn't include data: URL, as data: URLs will be explicitly marked
// as not secure.
const char* const kPseudoUrls[] = {
    "blob:http://test/some-guid", "filesystem:http://test/some-guid",
};

bool IsOriginSecure(const GURL& url) {
  return url == kHttpsUrl;
}

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
        is_incognito_(false),
        is_error_page_(false),
        is_view_source_(false),
        has_policy_certificate_(false) {}
  virtual ~TestSecurityStateHelper() {}

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
  void set_password_field_shown(bool password_field_shown) {
    insecure_input_events_.password_field_shown = password_field_shown;
  }
  void set_credit_card_field_edited(bool credit_card_field_edited) {
    insecure_input_events_.credit_card_field_edited = credit_card_field_edited;
  }
  void set_is_incognito(bool is_incognito) { is_incognito_ = is_incognito; }

  void set_is_error_page(bool is_error_page) { is_error_page_ = is_error_page; }

  void set_is_view_source(bool is_view_source) {
    is_view_source_ = is_view_source;
  }

  void set_insecure_field_edit(bool insecure_field_edit) {
    insecure_input_events_.insecure_field_edited = insecure_field_edit;
  }
  void set_has_policy_certificate(bool has_policy_cert) {
    has_policy_certificate_ = has_policy_cert;
  }
  void SetUrl(const GURL& url) { url_ = url; }

  std::unique_ptr<VisibleSecurityState> GetVisibleSecurityState() const {
    auto state = std::make_unique<VisibleSecurityState>();
    state->connection_info_initialized = true;
    state->url = url_;
    state->certificate = cert_;
    state->cert_status = cert_status_;
    state->connection_status = connection_status_;
    state->security_bits = 256;
    state->displayed_mixed_content = displayed_mixed_content_;
    state->contained_mixed_form = contained_mixed_form_;
    state->ran_mixed_content = ran_mixed_content_;
    state->malicious_content_status = malicious_content_status_;
    state->is_incognito = is_incognito_;
    state->is_error_page = is_error_page_;
    state->is_view_source = is_view_source_;
    state->insecure_input_events = insecure_input_events_;
    return state;
  }

  void GetSecurityInfo(SecurityInfo* security_info) const {
    security_state::GetSecurityInfo(GetVisibleSecurityState(),
                                    has_policy_certificate_,
                                    base::Bind(&IsOriginSecure), security_info);
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
  bool is_incognito_;
  bool is_error_page_;
  bool is_view_source_;
  bool has_policy_certificate_;
  InsecureInputEventData insecure_input_events_;
};

}  // namespace

// Tests that SHA1-signed certificates, when not allowed by policy, downgrade
// the security state of the page to DANGEROUS.
TEST(SecurityStateTest, SHA1Blocked) {
  TestSecurityStateHelper helper;
  helper.AddCertStatus(net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM);
  helper.AddCertStatus(net::CERT_STATUS_SHA1_SIGNATURE_PRESENT);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.sha1_in_chain);
  EXPECT_EQ(DANGEROUS, security_info.security_level);

  // Ensure that policy-installed certificates do not interfere.
  helper.set_has_policy_certificate(true);
  SecurityInfo policy_cert_security_info;
  helper.GetSecurityInfo(&policy_cert_security_info);
  EXPECT_TRUE(policy_cert_security_info.sha1_in_chain);
  EXPECT_EQ(DANGEROUS, policy_cert_security_info.security_level);
}

// Tests that SHA1-signed certificates, when allowed by policy, downgrade the
// security state of the page to NONE.
TEST(SecurityStateTest, SHA1Warning) {
  TestSecurityStateHelper helper;
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.sha1_in_chain);
  EXPECT_EQ(NONE, security_info.security_level);

  // Ensure that policy-installed certificates do not interfere.
  helper.set_has_policy_certificate(true);
  SecurityInfo policy_cert_security_info;
  helper.GetSecurityInfo(&policy_cert_security_info);
  EXPECT_TRUE(policy_cert_security_info.sha1_in_chain);
  EXPECT_EQ(NONE, policy_cert_security_info.security_level);
}

// Tests that SHA1-signed certificates, when allowed by policy, don't interfere
// with the handling of mixed content.
TEST(SecurityStateTest, SHA1WarningMixedContent) {
  TestSecurityStateHelper helper;
  helper.set_displayed_mixed_content(true);
  SecurityInfo security_info1;
  helper.GetSecurityInfo(&security_info1);
  EXPECT_TRUE(security_info1.sha1_in_chain);
  EXPECT_EQ(CONTENT_STATUS_DISPLAYED, security_info1.mixed_content_status);
  EXPECT_EQ(NONE, security_info1.security_level);

  helper.set_displayed_mixed_content(false);
  helper.set_ran_mixed_content(true);
  SecurityInfo security_info2;
  helper.GetSecurityInfo(&security_info2);
  EXPECT_TRUE(security_info2.sha1_in_chain);
  EXPECT_EQ(CONTENT_STATUS_RAN, security_info2.mixed_content_status);
  EXPECT_EQ(DANGEROUS, security_info2.security_level);
}

// Tests that SHA1-signed certificates, when allowed by policy,
// don't interfere with the handling of major cert errors.
TEST(SecurityStateTest, SHA1WarningBrokenHTTPS) {
  TestSecurityStateHelper helper;
  helper.AddCertStatus(net::CERT_STATUS_DATE_INVALID);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.sha1_in_chain);
  EXPECT_EQ(DANGEROUS, security_info.security_level);
}

// Tests that |security_info.is_secure_protocol_and_ciphersuite| is
// computed correctly.
TEST(SecurityStateTest, SecureProtocolAndCiphersuite) {
  TestSecurityStateHelper helper;
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  helper.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  helper.SetCipherSuite(ciphersuite);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(net::OBSOLETE_SSL_NONE, security_info.obsolete_ssl_status);
}

TEST(SecurityStateTest, NonsecureProtocol) {
  TestSecurityStateHelper helper;
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  helper.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_1
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  helper.SetCipherSuite(ciphersuite);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(net::OBSOLETE_SSL_MASK_PROTOCOL, security_info.obsolete_ssl_status);
}

TEST(SecurityStateTest, NonsecureCiphersuite) {
  TestSecurityStateHelper helper;
  // TLS_RSA_WITH_AES_128_CCM_8 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc0a0;
  helper.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  helper.SetCipherSuite(ciphersuite);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(net::OBSOLETE_SSL_MASK_KEY_EXCHANGE | net::OBSOLETE_SSL_MASK_CIPHER,
            security_info.obsolete_ssl_status);
}

// Tests that the malware/phishing status is set, and it overrides valid HTTPS.
TEST(SecurityStateTest, MalwareOverride) {
  TestSecurityStateHelper helper;
  // TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 from
  // http://www.iana.org/assignments/tls-parameters/tls-parameters.xml#tls-parameters-4
  const uint16_t ciphersuite = 0xc02f;
  helper.set_connection_status(net::SSL_CONNECTION_VERSION_TLS1_2
                               << net::SSL_CONNECTION_VERSION_SHIFT);
  helper.SetCipherSuite(ciphersuite);

  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);

  helper.set_malicious_content_status(MALICIOUS_CONTENT_STATUS_MALWARE);
  helper.GetSecurityInfo(&security_info);

  EXPECT_EQ(MALICIOUS_CONTENT_STATUS_MALWARE,
            security_info.malicious_content_status);
  EXPECT_EQ(DANGEROUS, security_info.security_level);
}

// Tests that the malware/phishing status is set, even if other connection info
// is not available.
TEST(SecurityStateTest, MalwareWithoutConnectionState) {
  TestSecurityStateHelper helper;
  helper.set_malicious_content_status(
      MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
            security_info.malicious_content_status);
  EXPECT_EQ(DANGEROUS, security_info.security_level);
}

// Tests that pseudo URLs always cause an HTTP_SHOW_WARNING to be shown,
// regardless of whether a password or credit card field was displayed.
TEST(SecurityStateTest, AlwaysWarnOnDataUrls) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL("data:text/html,<html>test</html>"));
  helper.set_password_field_shown(false);
  helper.set_credit_card_field_edited(false);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.insecure_input_events.password_field_shown);
  EXPECT_FALSE(security_info.insecure_input_events.credit_card_field_edited);
  EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
}

// Tests that FTP URLs always cause an HTTP_SHOW_WARNING to be shown,
// regardless of whether a password or credit card field was displayed.
TEST(SecurityStateTest, AlwaysWarnOnFtpUrls) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL("ftp://example.test/"));
  helper.set_password_field_shown(false);
  helper.set_credit_card_field_edited(false);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.insecure_input_events.password_field_shown);
  EXPECT_FALSE(security_info.insecure_input_events.credit_card_field_edited);
  EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
}

// Tests that password fields cause the security level to be downgraded
// to HTTP_SHOW_WARNING.
TEST(SecurityStateTest, PasswordFieldWarning) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));
  helper.set_password_field_shown(true);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.insecure_input_events.password_field_shown);
  EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
}

// Tests that the security level is downgraded to HTTP_SHOW_WARNING on pseudo
// URLs.
TEST(SecurityStateTest, WarningOnPseudoUrls) {
  for (const char* const url : kPseudoUrls) {
    TestSecurityStateHelper helper;
    helper.SetUrl(GURL(url));
    SecurityInfo security_info;
    helper.GetSecurityInfo(&security_info);
    EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
  }
}

// Tests that credit card fields cause the security level to be downgraded
// to HTTP_SHOW_WARNING.
TEST(SecurityStateTest, CreditCardFieldWarning) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));
  helper.set_credit_card_field_edited(true);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_TRUE(security_info.insecure_input_events.credit_card_field_edited);
  EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
}

// Tests that neither |password_field_shown| nor
// |credit_card_field_edited| is set when the corresponding
// VisibleSecurityState flags are not set.
TEST(SecurityStateTest, PrivateUserDataNotSet) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_FALSE(security_info.insecure_input_events.password_field_shown);
  EXPECT_FALSE(security_info.insecure_input_events.credit_card_field_edited);
  EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
}

// Tests that neither |password_field_shown| nor
// |credit_card_field_edited| is set on pseudo URLs when the
// corresponding VisibleSecurityState flags are not set.
TEST(SecurityStateTest, PrivateUserDataNotSetOnPseudoUrls) {
  for (const char* const url : kPseudoUrls) {
    TestSecurityStateHelper helper;
    helper.SetUrl(GURL(url));
    SecurityInfo security_info;
    helper.GetSecurityInfo(&security_info);
    EXPECT_FALSE(security_info.insecure_input_events.password_field_shown);
    EXPECT_FALSE(security_info.insecure_input_events.credit_card_field_edited);
    EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
  }
}

// Tests that if |is_view_source| NONE is returned for a secure site.
TEST(SecurityStateTest, ViewSourceRemovesSecure) {
  TestSecurityStateHelper helper;
  SecurityInfo security_info;
  helper.set_cert_status(0);
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(SECURE, security_info.security_level);
  helper.set_is_view_source(true);
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(NONE, security_info.security_level);
}

// Tests that if |is_view_source|, DANGEROUS is still returned for a site
// flagged by SafeBrowsing.
TEST(SecurityStateTest, ViewSourceKeepsWarning) {
  TestSecurityStateHelper helper;
  helper.set_malicious_content_status(
      MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING);
  helper.set_is_view_source(true);
  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
            security_info.malicious_content_status);
  EXPECT_EQ(DANGEROUS, security_info.security_level);
}

// Tests that |incognito_downgraded_security_level| is set only when the
// corresponding VisibleSecurityState flag is set. The incognito downgrade is
// only performed when the HTTP-Bad feature is disabled.
TEST(SecurityStateTest, IncognitoFlagPropagates) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));
  SecurityInfo security_info;

  {
    // When the feature is disabled, the downgraded flag should be set for
    // incognito http pages.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        security_state::features::kMarkHttpAsFeature);
    helper.set_is_incognito(false);
    helper.GetSecurityInfo(&security_info);
    EXPECT_FALSE(security_info.incognito_downgraded_security_level);

    helper.set_is_incognito(true);
    helper.GetSecurityInfo(&security_info);
    EXPECT_TRUE(security_info.incognito_downgraded_security_level);
  }

  {
    // When the feature is enabled, the downgraded flag should never be set.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        security_state::features::kMarkHttpAsFeature);
    helper.set_is_incognito(false);
    helper.GetSecurityInfo(&security_info);
    EXPECT_FALSE(security_info.incognito_downgraded_security_level);

    helper.set_is_incognito(true);
    helper.GetSecurityInfo(&security_info);
    EXPECT_FALSE(security_info.incognito_downgraded_security_level);
  }
}

TEST(SecurityStateTest, DetectSubjectAltName) {
  TestSecurityStateHelper helper;

  // Ensure subjectAltName is detected as present when the cert includes it.
  SecurityInfo san_security_info;
  helper.GetSecurityInfo(&san_security_info);
  EXPECT_FALSE(san_security_info.cert_missing_subject_alt_name);

  // Ensure subjectAltName is detected as missing when the cert doesn't
  // include it.
  scoped_refptr<net::X509Certificate> cert = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "salesforce_com_test.pem");
  ASSERT_TRUE(cert);
  helper.SetCertificate(std::move(cert));

  SecurityInfo no_san_security_info;
  helper.GetSecurityInfo(&no_san_security_info);
  EXPECT_TRUE(no_san_security_info.cert_missing_subject_alt_name);
}

// Tests that a mixed form is reflected in the SecurityInfo.
TEST(SecurityStateTest, MixedForm) {
  TestSecurityStateHelper helper;

  SecurityInfo no_mixed_form_security_info;
  helper.GetSecurityInfo(&no_mixed_form_security_info);
  EXPECT_FALSE(no_mixed_form_security_info.contained_mixed_form);

  helper.set_contained_mixed_form(true);

  // Verify that a mixed form downgrades the security level.
  SecurityInfo mixed_form_security_info;
  helper.GetSecurityInfo(&mixed_form_security_info);
  EXPECT_TRUE(mixed_form_security_info.contained_mixed_form);
  EXPECT_EQ(CONTENT_STATUS_NONE, mixed_form_security_info.mixed_content_status);
  EXPECT_EQ(NONE, mixed_form_security_info.security_level);

  // Ensure that active mixed content trumps the mixed form warning.
  helper.set_ran_mixed_content(true);
  SecurityInfo mixed_form_and_active_security_info;
  helper.GetSecurityInfo(&mixed_form_and_active_security_info);
  EXPECT_TRUE(mixed_form_and_active_security_info.contained_mixed_form);
  EXPECT_EQ(CONTENT_STATUS_RAN,
            mixed_form_and_active_security_info.mixed_content_status);
  EXPECT_EQ(DANGEROUS, mixed_form_and_active_security_info.security_level);
}

// Tests that policy-installed-certificates do not interfere with mixed content
// notifications.
TEST(SecurityStateTest, MixedContentWithPolicyCertificate) {
  TestSecurityStateHelper helper;

  helper.set_has_policy_certificate(true);
  helper.set_cert_status(0);

  {
    // Verify that if no mixed content is present, the policy-installed
    // certificate is recorded.
    SecurityInfo no_mixed_content_security_info;
    helper.GetSecurityInfo(&no_mixed_content_security_info);
    EXPECT_FALSE(no_mixed_content_security_info.contained_mixed_form);
    EXPECT_EQ(CONTENT_STATUS_NONE,
              no_mixed_content_security_info.mixed_content_status);
    EXPECT_EQ(SECURE_WITH_POLICY_INSTALLED_CERT,
              no_mixed_content_security_info.security_level);
  }

  {
    // Verify that a mixed form downgrades the security level.
    SecurityInfo mixed_form_security_info;
    helper.set_contained_mixed_form(true);
    helper.GetSecurityInfo(&mixed_form_security_info);
    EXPECT_TRUE(mixed_form_security_info.contained_mixed_form);
    EXPECT_EQ(CONTENT_STATUS_NONE,
              mixed_form_security_info.mixed_content_status);
    EXPECT_EQ(NONE, mixed_form_security_info.security_level);
  }

  {
    // Verify that passive mixed content downgrades the security level.
    helper.set_contained_mixed_form(false);
    helper.set_displayed_mixed_content(true);
    SecurityInfo passive_mixed_security_info;
    helper.GetSecurityInfo(&passive_mixed_security_info);
    EXPECT_EQ(CONTENT_STATUS_DISPLAYED,
              passive_mixed_security_info.mixed_content_status);
    EXPECT_EQ(NONE, passive_mixed_security_info.security_level);
  }

  {
    // Ensure that active mixed content downgrades the security level.
    helper.set_contained_mixed_form(false);
    helper.set_displayed_mixed_content(false);
    helper.set_ran_mixed_content(true);
    SecurityInfo active_mixed_security_info;
    helper.GetSecurityInfo(&active_mixed_security_info);
    EXPECT_EQ(CONTENT_STATUS_RAN,
              active_mixed_security_info.mixed_content_status);
    EXPECT_EQ(DANGEROUS, active_mixed_security_info.security_level);
  }
}

// Tests that a field edit is reflected in the SecurityInfo.
TEST(SecurityStateTest, FieldEdit) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));

  {
    // Test that a warning is shown on field edits, when the feature is
    // disabled.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndDisableFeature(
        security_state::features::kMarkHttpAsFeature);

    SecurityInfo no_field_edit_security_info;
    helper.GetSecurityInfo(&no_field_edit_security_info);
    EXPECT_FALSE(no_field_edit_security_info.insecure_input_events
                     .insecure_field_edited);
    EXPECT_FALSE(
        no_field_edit_security_info.field_edit_downgraded_security_level);
    EXPECT_EQ(NONE, no_field_edit_security_info.security_level);

    helper.set_insecure_field_edit(true);

    SecurityInfo security_info;
    helper.GetSecurityInfo(&security_info);
    EXPECT_TRUE(security_info.insecure_input_events.insecure_field_edited);
    EXPECT_TRUE(security_info.field_edit_downgraded_security_level);
    EXPECT_EQ(HTTP_SHOW_WARNING, security_info.security_level);
  }

  {
    // Test that the default enabled configuration shows the dangerous warning
    // on field edits.
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitAndEnableFeature(
        security_state::features::kMarkHttpAsFeature);

    SecurityInfo security_info;
    helper.GetSecurityInfo(&security_info);
    EXPECT_TRUE(security_info.insecure_input_events.insecure_field_edited);
    EXPECT_FALSE(security_info.field_edit_downgraded_security_level);
    EXPECT_EQ(DANGEROUS, security_info.security_level);
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
  EXPECT_TRUE(IsSslCertificateValid(SecurityLevel::EV_SECURE));
  EXPECT_TRUE(
      IsSslCertificateValid(SecurityLevel::SECURE_WITH_POLICY_INSTALLED_CERT));

  EXPECT_FALSE(IsSslCertificateValid(SecurityLevel::NONE));
  EXPECT_FALSE(IsSslCertificateValid(SecurityLevel::DANGEROUS));
  EXPECT_FALSE(IsSslCertificateValid(SecurityLevel::HTTP_SHOW_WARNING));
}

// Tests that HTTP_SHOW_WARNING is not set in incognito for error pages.
TEST(SecurityStateTest, IncognitoErrorPage) {
  TestSecurityStateHelper helper;
  SecurityInfo security_info;
  helper.SetUrl(GURL("http://nonexistent.test"));
  helper.set_is_incognito(true);
  helper.set_is_error_page(true);
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(SecurityLevel::NONE, security_info.security_level);
  EXPECT_FALSE(security_info.incognito_downgraded_security_level);

  // Sanity-check that if it's not an error page, the security level is
  // downgraded.
  helper.set_is_error_page(false);
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(SecurityLevel::HTTP_SHOW_WARNING, security_info.security_level);
}

// Tests that HTTP_SHOW_WARNING is set on normal http pages but DANGEROUS on
// form edits when the default feature configuration is enabled.
TEST(SecurityStateTest, WarningAndDangerousOnFormEdits) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      security_state::features::kMarkHttpAsFeature);

  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));

  {
    SecurityInfo security_info;
    helper.GetSecurityInfo(&security_info);
    EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  }

  {
    SecurityInfo security_info;
    helper.set_insecure_field_edit(true);
    helper.GetSecurityInfo(&security_info);
    EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  }

  {
    SecurityInfo security_info;
    helper.set_insecure_field_edit(false);
    helper.set_password_field_shown(true);
    helper.GetSecurityInfo(&security_info);
    EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  }
}

// Tests that HTTP_SHOW_WARNING is set on normal http pages but DANGEROUS on
// sensitive fields when the
// 'warning-and-dangerous-on-passwords-and-credit-cards' field trial
// configuration is enabled.
TEST(SecurityStateTest, WarningAndDangerousOnSensitiveFields) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      security_state::features::kMarkHttpAsFeature,
      {{security_state::features::kMarkHttpAsFeatureParameterName,
        security_state::features::
            kMarkHttpAsParameterWarningAndDangerousOnPasswordsAndCreditCards}});

  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));

  {
    SecurityInfo security_info;
    helper.GetSecurityInfo(&security_info);
    EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  }

  {
    SecurityInfo security_info;
    helper.set_insecure_field_edit(true);
    helper.GetSecurityInfo(&security_info);
    EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);
  }

  {
    SecurityInfo security_info;
    helper.set_insecure_field_edit(false);
    helper.set_password_field_shown(true);
    helper.GetSecurityInfo(&security_info);
    EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  }

  {
    SecurityInfo security_info;
    helper.set_password_field_shown(false);
    helper.set_credit_card_field_edited(true);
    helper.GetSecurityInfo(&security_info);
    EXPECT_EQ(security_state::DANGEROUS, security_info.security_level);
  }
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

  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  EXPECT_EQ(MALICIOUS_CONTENT_STATUS_NONE,
            security_info.malicious_content_status);

  helper.set_malicious_content_status(MALICIOUS_CONTENT_STATUS_BILLING);
  helper.GetSecurityInfo(&security_info);

  EXPECT_EQ(MALICIOUS_CONTENT_STATUS_BILLING,
            security_info.malicious_content_status);
  EXPECT_EQ(DANGEROUS, security_info.security_level);
}

// Tests that the billing status is set, and it overrides invalid HTTPS.
TEST(SecurityStateTest, BillingOverridesHTTPWarning) {
  TestSecurityStateHelper helper;
  helper.SetUrl(GURL(kHttpUrl));

  SecurityInfo security_info;
  helper.GetSecurityInfo(&security_info);
  // Expect to see a warning for HTTP first.
  EXPECT_EQ(security_state::HTTP_SHOW_WARNING, security_info.security_level);

  // Now mark the URL as matching the billing list.
  helper.set_malicious_content_status(MALICIOUS_CONTENT_STATUS_BILLING);
  helper.GetSecurityInfo(&security_info);
  // Expect to see a warning for billing now.
  EXPECT_EQ(MALICIOUS_CONTENT_STATUS_BILLING,
            security_info.malicious_content_status);
  EXPECT_EQ(DANGEROUS, security_info.security_level);
}

}  // namespace security_state
