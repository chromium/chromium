// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/content/content_utils.h"

#include <string>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/security_state/core/features.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/security_style_explanation.h"
#include "content/public/browser/security_style_explanations.h"
#include "crypto/rsa_private_key.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/x509_util.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using security_state::GetSecurityStyle;

// Tests that malicious safe browsing data in VisibleSecurityState triggers an
// appropriate summary in SecurityStyleExplanations.
TEST(SecurityStateContentUtilsTest, GetSecurityStyleForSafeBrowsing) {
  content::SecurityStyleExplanations explanations;
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status = 0;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");
  visible_security_state.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_MALWARE;

  visible_security_state.displayed_mixed_content = true;
  visible_security_state.ran_mixed_content = true;
  GetSecurityStyle(security_state::DANGEROUS, visible_security_state,
                   &explanations);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SAFEBROWSING_WARNING),
            explanations.summary);
}

// Tests that a non-cryptographic secure origin triggers an appropriate summary
// in SecurityStyleExplanations.
TEST(SecurityStateContentUtilsTest,
     GetSecurityStyleForNonCryptographicSecureOrigin) {
  content::SecurityStyleExplanations explanations;
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status = 0;
  visible_security_state.url = GURL("chrome://test");
  GetSecurityStyle(security_state::NONE, visible_security_state, &explanations);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_NON_CRYPTO_SECURE_SUMMARY),
            explanations.summary);
}

// Tests that non cert errors result in an appropriate summary in
// SecurityStyleExplanations.
TEST(SecurityStateContentUtilsTest, GetSecurityStyleForNonCertErrors) {
  content::SecurityStyleExplanations explanations;
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status = 0;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");

  visible_security_state.is_error_page = true;
  GetSecurityStyle(security_state::NONE, visible_security_state, &explanations);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_ERROR_PAGE_SUMMARY),
            explanations.summary);
}

// Tests that malicious safe browsing data triggers the Safe Browsing warning
// summary when |is_error_page| is set to true.
TEST(SecurityStateContentUtilsTest,
     GetSecurityStyleForSafeBrowsingNonCertError) {
  content::SecurityStyleExplanations explanations;
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status = 0;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");

  visible_security_state.is_error_page = true;
  visible_security_state.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_MALWARE;

  GetSecurityStyle(security_state::DANGEROUS, visible_security_state,
                   &explanations);
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SAFEBROWSING_WARNING),
            explanations.summary);
}

bool FindSecurityStyleExplanation(
    const std::vector<content::SecurityStyleExplanation>& explanations,
    const std::string& title,
    const std::string& summary,
    content::SecurityStyleExplanation* explanation) {
  for (const auto& entry : explanations) {
    if (entry.title == title && entry.summary == summary) {
      *explanation = entry;
      return true;
    }
  }

  return false;
}

// Test that connection explanations are formatted as expected. Note the strings
// are not translated and so will be the same in any locale.
TEST(SecurityStateContentUtilsTest, ConnectionExplanation) {
  // Test a modern configuration with a key exchange group.
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status =
      net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");
  net::SSLConnectionStatusSetCipherSuite(
      0xcca8 /* TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 */,
      &visible_security_state.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &visible_security_state.connection_status);
  visible_security_state.key_exchange_group = 29;  // X25519

  std::string connection_title =
      l10n_util::GetStringUTF8(IDS_SSL_CONNECTION_TITLE);

  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::NONE, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.secure_explanations, connection_title,
        l10n_util::GetStringUTF8(IDS_SECURE_SSL_SUMMARY), &explanation));
    EXPECT_EQ(
        "The connection to this site is encrypted and authenticated using TLS "
        "1.2, ECDHE_RSA with X25519, and CHACHA20_POLY1305.",
        explanation.description);
  }

  // Some older cache entries may be missing the key exchange group, despite
  // having a cipher which should supply one.
  visible_security_state.key_exchange_group = 0;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::NONE, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.secure_explanations, connection_title,
        l10n_util::GetStringUTF8(IDS_SECURE_SSL_SUMMARY), &explanation));
    EXPECT_EQ(
        "The connection to this site is encrypted and authenticated using TLS "
        "1.2, ECDHE_RSA, and CHACHA20_POLY1305.",
        explanation.description);
  }

  // TLS 1.3 ciphers use the key exchange group exclusively.
  net::SSLConnectionStatusSetCipherSuite(
      0x1301 /* TLS_AES_128_GCM_SHA256 */,
      &visible_security_state.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_3,
                                     &visible_security_state.connection_status);
  visible_security_state.key_exchange_group = 29;  // X25519
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::NONE, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;

    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.secure_explanations, connection_title,
        l10n_util::GetStringUTF8(IDS_SECURE_SSL_SUMMARY), &explanation));
    EXPECT_EQ(
        "The connection to this site is encrypted and authenticated using TLS "
        "1.3, X25519, and AES_128_GCM.",
        explanation.description);
  }
}

bool IsProtocolRecommendation(const std::string& recommendation,
                              const std::string& bad_protocol) {
  return recommendation.find(bad_protocol) != std::string::npos &&
         recommendation.find("TLS 1.2") != std::string::npos;
}

bool IsKeyExchangeRecommendation(const std::string& recommendation) {
  return recommendation.find("RSA") != std::string::npos &&
         recommendation.find("ECDHE") != std::string::npos;
}

bool IsCipherRecommendation(const std::string& recommendation,
                            const std::string& bad_cipher) {
  return recommendation.find(bad_cipher) != std::string::npos &&
         recommendation.find("GCM") != std::string::npos;
}

bool IsSignatureRecommendation(const std::string& recommendation) {
  return recommendation.find("SHA-1") != std::string::npos &&
         recommendation.find("SHA-2") != std::string::npos;
}

// Test that obsolete connection explanations are formatted as expected.
TEST(SecurityStateContentUtilsTest, ObsoleteConnectionExplanation) {
  // Obsolete cipher.
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status =
      net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");
  net::SSLConnectionStatusSetCipherSuite(
      0xc013 /* TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA */,
      &visible_security_state.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &visible_security_state.connection_status);
  visible_security_state.key_exchange_group = 29;  // X25519
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::SECURE, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.info_explanations,
        l10n_util::GetStringUTF8(IDS_SSL_CONNECTION_TITLE),
        l10n_util::GetStringUTF8(IDS_OBSOLETE_SSL_SUMMARY), &explanation));
    EXPECT_EQ(
        "The connection to this site is encrypted and authenticated using TLS "
        "1.2, ECDHE_RSA with X25519, and AES_128_CBC with HMAC-SHA1.",
        explanation.description);

    ASSERT_EQ(1u, explanation.recommendations.size());
    EXPECT_TRUE(
        IsCipherRecommendation(explanation.recommendations[0], "AES_128_CBC"))
        << explanation.recommendations[0];
  }

  // Obsolete cipher and signature.
  visible_security_state.peer_signature_algorithm = 0x0201;  // rsa_pkcs1_sha1
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::SECURE, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.info_explanations,
        l10n_util::GetStringUTF8(IDS_SSL_CONNECTION_TITLE),
        l10n_util::GetStringUTF8(IDS_OBSOLETE_SSL_SUMMARY), &explanation));

    ASSERT_EQ(2u, explanation.recommendations.size());
    EXPECT_TRUE(
        IsCipherRecommendation(explanation.recommendations[0], "AES_128_CBC"))
        << explanation.recommendations[0];
    EXPECT_TRUE(IsSignatureRecommendation(explanation.recommendations[1]))
        << explanation.recommendations[1];
  }

  // Obsolete protocol version and cipher.
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1,
                                     &visible_security_state.connection_status);
  // TLS 1.0 doesn't negotiate a signature algorithm.
  visible_security_state.peer_signature_algorithm = 0;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::SECURE, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.info_explanations,
        l10n_util::GetStringUTF8(IDS_SSL_CONNECTION_TITLE),
        l10n_util::GetStringUTF8(IDS_OBSOLETE_SSL_SUMMARY), &explanation));

    ASSERT_EQ(2u, explanation.recommendations.size());
    EXPECT_TRUE(
        IsProtocolRecommendation(explanation.recommendations[0], "TLS 1.0"))
        << explanation.recommendations[0];
    EXPECT_TRUE(
        IsCipherRecommendation(explanation.recommendations[1], "AES_128_CBC"))
        << explanation.recommendations[1];
  }

  // Obsolete protocol version, cipher, and key exchange.
  net::SSLConnectionStatusSetCipherSuite(
      0x000a /* TLS_RSA_WITH_3DES_EDE_CBC_SHA */,
      &visible_security_state.connection_status);
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::SECURE, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.info_explanations,
        l10n_util::GetStringUTF8(IDS_SSL_CONNECTION_TITLE),
        l10n_util::GetStringUTF8(IDS_OBSOLETE_SSL_SUMMARY), &explanation));

    ASSERT_EQ(3u, explanation.recommendations.size());
    EXPECT_TRUE(
        IsProtocolRecommendation(explanation.recommendations[0], "TLS 1.0"))
        << explanation.recommendations[0];
    EXPECT_TRUE(IsKeyExchangeRecommendation(explanation.recommendations[1]))
        << explanation.recommendations[1];
    EXPECT_TRUE(
        IsCipherRecommendation(explanation.recommendations[2], "3DES_EDE_CBC"))
        << explanation.recommendations[2];
  }

  // Obsolete key exchange.
  net::SSLConnectionStatusSetCipherSuite(
      0x009c /* TLS_RSA_WITH_AES_128_GCM_SHA256 */,
      &visible_security_state.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &visible_security_state.connection_status);
  visible_security_state.peer_signature_algorithm =
      0x0804;  // rsa_pss_rsae_sha256
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::SECURE, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.info_explanations,
        l10n_util::GetStringUTF8(IDS_SSL_CONNECTION_TITLE),
        l10n_util::GetStringUTF8(IDS_OBSOLETE_SSL_SUMMARY), &explanation));

    ASSERT_EQ(1u, explanation.recommendations.size());
    EXPECT_TRUE(IsKeyExchangeRecommendation(explanation.recommendations[0]))
        << explanation.recommendations[0];
  }

  // Obsolete signature.
  net::SSLConnectionStatusSetCipherSuite(
      0xc02f /* TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 */,
      &visible_security_state.connection_status);
  visible_security_state.peer_signature_algorithm = 0x0201;  // rsa_pkcs1_sha1
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::SECURE, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_TRUE(FindSecurityStyleExplanation(
        explanations.info_explanations,
        l10n_util::GetStringUTF8(IDS_SSL_CONNECTION_TITLE),
        l10n_util::GetStringUTF8(IDS_OBSOLETE_SSL_SUMMARY), &explanation));

    ASSERT_EQ(1u, explanation.recommendations.size());
    EXPECT_TRUE(IsSignatureRecommendation(explanation.recommendations[0]))
        << explanation.recommendations[0];
  }
}

// Test that a secure content explanation is added as expected.
TEST(SecurityStateContentUtilsTest, SecureContentExplanation) {
  // Test a modern configuration with a key exchange group.
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status =
      net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");
  net::SSLConnectionStatusSetCipherSuite(
      0xcca8 /* TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 */,
      &visible_security_state.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &visible_security_state.connection_status);
  visible_security_state.key_exchange_group = 29;  // X25519

  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::SECURE, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    EXPECT_TRUE(FindSecurityStyleExplanation(
        explanations.secure_explanations,
        l10n_util::GetStringUTF8(IDS_RESOURCE_SECURITY_TITLE),
        l10n_util::GetStringUTF8(IDS_SECURE_RESOURCES_SUMMARY), &explanation));
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SECURE_RESOURCES_DESCRIPTION),
              explanation.description);
  }
}

// Test that mixed content explanations are added as expected.
TEST(SecurityStateContentUtilsTest, MixedContentExplanations) {
  // Test a modern configuration with a key exchange group.
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status =
      net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");
  net::SSLConnectionStatusSetCipherSuite(
      0xcca8 /* TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 */,
      &visible_security_state.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &visible_security_state.connection_status);
  visible_security_state.key_exchange_group = 29;  // X25519

  std::string content_title =
      l10n_util::GetStringUTF8(IDS_RESOURCE_SECURITY_TITLE);

  visible_security_state.displayed_mixed_content = true;
  visible_security_state.ran_mixed_content = true;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::DANGEROUS, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    EXPECT_TRUE(FindSecurityStyleExplanation(
        explanations.neutral_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_MIXED_PASSIVE_CONTENT_SUMMARY),
        &explanation));
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_MIXED_PASSIVE_CONTENT_DESCRIPTION),
              explanation.description);
    EXPECT_TRUE(FindSecurityStyleExplanation(
        explanations.insecure_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_MIXED_ACTIVE_CONTENT_SUMMARY),
        &explanation));
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_MIXED_ACTIVE_CONTENT_DESCRIPTION),
              explanation.description);
  }

  visible_security_state.displayed_mixed_content = true;
  visible_security_state.ran_mixed_content = false;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::NONE, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    EXPECT_TRUE(FindSecurityStyleExplanation(
        explanations.neutral_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_MIXED_PASSIVE_CONTENT_SUMMARY),
        &explanation));
    EXPECT_FALSE(FindSecurityStyleExplanation(
        explanations.insecure_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_MIXED_ACTIVE_CONTENT_SUMMARY),
        &explanation));
  }

  visible_security_state.displayed_mixed_content = false;
  visible_security_state.ran_mixed_content = true;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::DANGEROUS, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    EXPECT_FALSE(FindSecurityStyleExplanation(
        explanations.neutral_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_MIXED_PASSIVE_CONTENT_SUMMARY),
        &explanation));
    EXPECT_TRUE(FindSecurityStyleExplanation(
        explanations.insecure_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_MIXED_ACTIVE_CONTENT_SUMMARY),
        &explanation));
  }

  visible_security_state.contained_mixed_form = true;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::DANGEROUS, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    EXPECT_TRUE(FindSecurityStyleExplanation(
        explanations.neutral_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_NON_SECURE_FORM_SUMMARY), &explanation));
    EXPECT_EQ(l10n_util::GetStringUTF8(IDS_NON_SECURE_FORM_DESCRIPTION),
              explanation.description);
  }
}

// Test that cert error explanations are formatted as expected.
TEST(SecurityStateContentUtilsTest, CertErrorContentExplanations) {
  // Test a modern configuration with a key exchange group.
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status = 0;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");
  net::SSLConnectionStatusSetCipherSuite(
      0xcca8 /* TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 */,
      &visible_security_state.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &visible_security_state.connection_status);
  visible_security_state.key_exchange_group = 29;  // X25519

  std::string content_title =
      l10n_util::GetStringUTF8(IDS_RESOURCE_SECURITY_TITLE);

  visible_security_state.displayed_content_with_cert_errors = true;
  visible_security_state.ran_content_with_cert_errors = true;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::DANGEROUS, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    EXPECT_TRUE(FindSecurityStyleExplanation(
        explanations.neutral_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_CERT_ERROR_PASSIVE_CONTENT_SUMMARY),
        &explanation));
    EXPECT_EQ(
        l10n_util::GetStringUTF8(IDS_CERT_ERROR_PASSIVE_CONTENT_DESCRIPTION),
        explanation.description);
    EXPECT_TRUE(FindSecurityStyleExplanation(
        explanations.insecure_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_CERT_ERROR_ACTIVE_CONTENT_SUMMARY),
        &explanation));
    EXPECT_EQ(
        l10n_util::GetStringUTF8(IDS_CERT_ERROR_ACTIVE_CONTENT_DESCRIPTION),
        explanation.description);
  }

  visible_security_state.displayed_content_with_cert_errors = true;
  visible_security_state.ran_content_with_cert_errors = false;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::NONE, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    EXPECT_TRUE(FindSecurityStyleExplanation(
        explanations.neutral_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_CERT_ERROR_PASSIVE_CONTENT_SUMMARY),
        &explanation));
    ASSERT_FALSE(FindSecurityStyleExplanation(
        explanations.insecure_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_CERT_ERROR_ACTIVE_CONTENT_SUMMARY),
        &explanation));
  }

  visible_security_state.displayed_content_with_cert_errors = false;
  visible_security_state.ran_content_with_cert_errors = true;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::DANGEROUS, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    ASSERT_FALSE(FindSecurityStyleExplanation(
        explanations.neutral_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_CERT_ERROR_PASSIVE_CONTENT_SUMMARY),
        &explanation));
    EXPECT_TRUE(FindSecurityStyleExplanation(
        explanations.insecure_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_CERT_ERROR_ACTIVE_CONTENT_SUMMARY),
        &explanation));
    EXPECT_EQ(
        l10n_util::GetStringUTF8(IDS_CERT_ERROR_ACTIVE_CONTENT_DESCRIPTION),
        explanation.description);
  }
}

// Test that all mixed content explanations can appear together.
TEST(SecurityStateContentUtilsTest, MixedContentAndCertErrorExplanations) {
  // Test a modern configuration with a key exchange group.
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status = 0;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");
  net::SSLConnectionStatusSetCipherSuite(
      0xcca8 /* TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 */,
      &visible_security_state.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &visible_security_state.connection_status);
  visible_security_state.key_exchange_group = 29;  // X25519

  std::string content_title =
      l10n_util::GetStringUTF8(IDS_RESOURCE_SECURITY_TITLE);

  visible_security_state.displayed_mixed_content = true;
  visible_security_state.ran_mixed_content = true;
  visible_security_state.displayed_content_with_cert_errors = true;
  visible_security_state.ran_content_with_cert_errors = true;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_state::DANGEROUS, visible_security_state,
                     &explanations);
    content::SecurityStyleExplanation explanation;
    EXPECT_TRUE(FindSecurityStyleExplanation(
        explanations.neutral_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_MIXED_PASSIVE_CONTENT_SUMMARY),
        &explanation));
    EXPECT_TRUE(FindSecurityStyleExplanation(
        explanations.insecure_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_MIXED_ACTIVE_CONTENT_SUMMARY),
        &explanation));
    EXPECT_TRUE(FindSecurityStyleExplanation(
        explanations.neutral_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_CERT_ERROR_PASSIVE_CONTENT_SUMMARY),
        &explanation));
    EXPECT_TRUE(FindSecurityStyleExplanation(
        explanations.insecure_explanations, content_title,
        l10n_util::GetStringUTF8(IDS_CERT_ERROR_ACTIVE_CONTENT_SUMMARY),
        &explanation));
  }
}

// Tests that a security level of WARNING produces
// blink::kSecurityStyleNeutral.
TEST(SecurityStateContentUtilsTest, HTTPWarning) {
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.url = GURL("http://scheme-is-not-cryptographic.test");
  content::SecurityStyleExplanations explanations;
  blink::SecurityStyle security_style = GetSecurityStyle(
      security_state::WARNING, visible_security_state, &explanations);
  EXPECT_EQ(blink::SecurityStyle::kNeutral, security_style);
  // Verify no explanation was shown.
  EXPECT_EQ(0u, explanations.neutral_explanations.size());
}

// Tests that a security level of DANGEROUS on an HTTP page with insecure form
// edits produces blink::SecurityStyleInsecure and an explanation.
TEST(SecurityStateContentUtilsTest, HTTPDangerous) {
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.url = GURL("http://scheme-is-not-cryptographic.test");
  content::SecurityStyleExplanations explanations;
  visible_security_state.insecure_input_events.insecure_field_edited = true;
  blink::SecurityStyle security_style = GetSecurityStyle(
      security_state::DANGEROUS, visible_security_state, &explanations);
  // Verify that the security style was downgraded and an explanation shown
  // because a form was edited.
  EXPECT_EQ(blink::SecurityStyle::kInsecureBroken, security_style);
  EXPECT_EQ(1u, explanations.insecure_explanations.size());
}

// Tests that an explanation is provided if a certificate is missing a
// subjectAltName extension containing a domain name or IP address.
TEST(SecurityStateContentUtilsTest, SubjectAltNameWarning) {
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status = 0;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");
  visible_security_state.certificate = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "salesforce_com_test.pem");
  ASSERT_TRUE(visible_security_state.certificate);

  content::SecurityStyleExplanations explanations;
  GetSecurityStyle(security_state::DANGEROUS, visible_security_state,
                   &explanations);
  // Verify that an explanation was shown for a missing subjectAltName.
  EXPECT_EQ(1u, explanations.insecure_explanations.size());

  explanations.insecure_explanations.clear();
  visible_security_state.certificate =
      net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  GetSecurityStyle(security_state::SECURE, visible_security_state,
                   &explanations);
  // Verify that no explanation is shown if the subjectAltName is present.
  EXPECT_EQ(0u, explanations.insecure_explanations.size());
}

// Tests that malicious safe browsing data in VisibleSecurityState causes an
// insecure explanation to be set.
TEST(SecurityStateContentUtilsTest, SafeBrowsingExplanation) {
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status = 0;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");
  visible_security_state.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_MALWARE;
  content::SecurityStyleExplanations explanations;
  GetSecurityStyle(security_state::DANGEROUS, visible_security_state,
                   &explanations);
  EXPECT_EQ(1u, explanations.insecure_explanations.size());
}

// Tests that a bad reputation warning in VisibleSecurityState causes an
// insecure explanation to be set.
TEST(SecurityStateContentUtilsTest, SafetyTipExplanation_BadReputation) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      security_state::features::kSafetyTipUI);

  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status = 0;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");
  visible_security_state.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_NONE;
  visible_security_state.safety_tip_info = {
      security_state::SafetyTipStatus::kBadReputation, GURL()};
  content::SecurityStyleExplanations explanations;
  GetSecurityStyle(security_state::WARNING, visible_security_state,
                   &explanations);

  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SECURITY_TAB_SAFETY_TIP_TITLE),
            explanations.summary);
  EXPECT_EQ(1u, explanations.insecure_explanations.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_SECURITY_TAB_SAFETY_TIP_BAD_REPUTATION_DESCRIPTION),
            explanations.insecure_explanations[0].description);
}

// Same as SafetyTipExplanation_BadReputation, but for lookalikes. Also checks
// that the explanation text contains the safe URL.
TEST(SecurityStateContentUtilsTest, SafetyTipExplanation_Lookalike) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      security_state::features::kSafetyTipUI);

  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status = 0;
  visible_security_state.url = GURL("https://lookalike.test");
  visible_security_state.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_NONE;
  visible_security_state.safety_tip_info = {
      security_state::SafetyTipStatus::kLookalike,
      GURL("http://good-site.test")};
  content::SecurityStyleExplanations explanations;
  GetSecurityStyle(security_state::WARNING, visible_security_state,
                   &explanations);

  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SECURITY_TAB_SAFETY_TIP_TITLE),
            explanations.summary);
  EXPECT_EQ(1u, explanations.insecure_explanations.size());
  EXPECT_EQ(l10n_util::GetStringFUTF8(
                IDS_SECURITY_TAB_SAFETY_TIP_LOOKALIKE_DESCRIPTION,
                base::ASCIIToUTF16("good-site.test")),
            explanations.insecure_explanations[0].description);
}

// Tests that a Safebrowsing warning and a bad reputation warning in
// VisibleSecurityState causes two insecure explanations to be set, while
// keeping the title SafeBrowsing related.
TEST(SecurityStateContentUtilsTest,
     SafetyTipExplanation_WithSafeBrowsingError) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      security_state::features::kSafetyTipUI);

  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status = 0;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");
  visible_security_state.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_MALWARE;
  visible_security_state.safety_tip_info = {
      security_state::SafetyTipStatus::kBadReputation, GURL()};
  content::SecurityStyleExplanations explanations;
  GetSecurityStyle(security_state::DANGEROUS, visible_security_state,
                   &explanations);
  // When there is also a SafeBrowsing warning, the title must be related to
  // SafeBrowsing.
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SAFEBROWSING_WARNING),
            explanations.summary);

  EXPECT_EQ(2u, explanations.insecure_explanations.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SAFEBROWSING_WARNING_SUMMARY),
            explanations.insecure_explanations[0].summary);
  EXPECT_EQ(l10n_util::GetStringUTF8(
                IDS_SECURITY_TAB_SAFETY_TIP_BAD_REPUTATION_SUMMARY),
            explanations.insecure_explanations[1].summary);
}

// Tests that a Safebrowsing warning and safety tip status of None in
// VisibleSecurityState causes only one insecure explanation to be set.
TEST(SecurityStateContentUtilsTest,
     SafetyTipExplanationNone_WithSafeBrowsingError) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      security_state::features::kSafetyTipUI);

  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status = 0;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");
  visible_security_state.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_MALWARE;
  visible_security_state.safety_tip_info = {
      security_state::SafetyTipStatus::kNone, GURL()};
  content::SecurityStyleExplanations explanations;
  GetSecurityStyle(security_state::DANGEROUS, visible_security_state,
                   &explanations);
  // When there is also a SafeBrowsing warning, the title must be related to
  // SafeBrowsing.
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SAFEBROWSING_WARNING),
            explanations.summary);

  EXPECT_EQ(1u, explanations.insecure_explanations.size());
  EXPECT_EQ(l10n_util::GetStringUTF8(IDS_SAFEBROWSING_WARNING_SUMMARY),
            explanations.insecure_explanations[0].summary);
}

// NSS requires that serial numbers be unique even for the same issuer;
// as all fake certificates will contain the same issuer name, it's
// necessary to ensure the serial number is unique, as otherwise
// NSS will fail to parse.
base::AtomicSequenceNumber g_serial_number;

scoped_refptr<net::X509Certificate> CreateFakeCert(
    base::TimeDelta time_until_expiration) {
  std::unique_ptr<crypto::RSAPrivateKey> unused_key;
  std::string cert_der;
  if (!net::x509_util::CreateKeyAndSelfSignedCert(
          "CN=Error", static_cast<uint32_t>(g_serial_number.GetNext()),
          base::Time::Now() - base::TimeDelta::FromMinutes(30),
          base::Time::Now() + time_until_expiration, &unused_key, &cert_der)) {
    return nullptr;
  }
  return net::X509Certificate::CreateFromBytes(cert_der.data(),
                                               cert_der.size());
}

// Tests that an info explanation is provided only if the certificate is
// expiring soon.
TEST(SecurityStateContentUtilsTest, ExpiringCertificateWarning) {
  security_state::VisibleSecurityState visible_security_state;
  visible_security_state.cert_status = 0;
  visible_security_state.url = GURL("https://scheme-is-cryptographic.test");

  // Check that an info explanation is provided if the certificate is expiring
  // in less than 48 hours.
  content::SecurityStyleExplanations explanations;
  visible_security_state.certificate = scoped_refptr<net::X509Certificate>(
      CreateFakeCert(base::TimeDelta::FromHours(30)));
  ASSERT_TRUE(visible_security_state.certificate);
  GetSecurityStyle(security_state::SECURE, visible_security_state,
                   &explanations);
  EXPECT_EQ(1u, explanations.info_explanations.size());

  // Check that no explanation is set if the certificate is expiring in more
  // than 48 hours.
  explanations.info_explanations.clear();
  visible_security_state.certificate = scoped_refptr<net::X509Certificate>(
      CreateFakeCert(base::TimeDelta::FromHours(72)));
  ASSERT_TRUE(visible_security_state.certificate);
  GetSecurityStyle(security_state::SECURE, visible_security_state,
                   &explanations);
  EXPECT_EQ(0u, explanations.info_explanations.size());

  // Check that no explanation is set if the certificate has already expired.
  explanations.info_explanations.clear();
  visible_security_state.certificate = scoped_refptr<net::X509Certificate>(
      CreateFakeCert(base::TimeDelta::FromHours(-10)));
  ASSERT_TRUE(visible_security_state.certificate);
  GetSecurityStyle(security_state::SECURE, visible_security_state,
                   &explanations);
  EXPECT_EQ(0u, explanations.info_explanations.size());
}

// Tests that an explanation using the shorter constructor sets the correct
// default values for other fields.
TEST(SecurityStateContentUtilsTest, DefaultSecurityStyleExplanation) {
  content::SecurityStyleExplanation explanation("title", "summary",
                                                "description");

  EXPECT_EQ(false, !!explanation.certificate);
  EXPECT_EQ(blink::WebMixedContentContextType::kNotMixedContent,
            explanation.mixed_content_type);
}

}  // namespace
