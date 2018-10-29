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

// Tests that SecurityInfo flags for subresources with certificate
// errors are reflected in the SecurityStyleExplanations produced by
// GetSecurityStyle.
TEST(SecurityStateContentUtilsTest, GetSecurityStyleForContentWithCertErrors) {
  content::SecurityStyleExplanations explanations;
  security_state::SecurityInfo security_info;
  security_info.cert_status = 0;
  security_info.scheme_is_cryptographic = true;

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_RAN;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_NONE;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);
}

// Tests that SecurityStyleExplanations for subresources with cert
// errors are *not* set when the main resource has major certificate
// errors. If the main resource has certificate errors, it would be
// duplicative/confusing to also report subresources with cert errors.
TEST(SecurityStateContentUtilsTest,
     SubresourcesAndMainResourceWithMajorCertErrors) {
  content::SecurityStyleExplanations explanations;
  security_state::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_DATE_INVALID;
  security_info.scheme_is_cryptographic = true;

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_RAN;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_NONE;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);
}

// Tests that SecurityStyleExplanations for subresources with cert
// errors are set when the main resource has only minor certificate
// errors. Minor errors on the main resource should not hide major
// errors on subresources.
TEST(SecurityStateContentUtilsTest,
     SubresourcesAndMainResourceWithMinorCertErrors) {
  content::SecurityStyleExplanations explanations;
  security_state::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  security_info.scheme_is_cryptographic = true;

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_RAN;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_TRUE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_TRUE(explanations.displayed_content_with_cert_errors);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_NONE;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.ran_content_with_cert_errors);
  EXPECT_FALSE(explanations.displayed_content_with_cert_errors);
}

// Tests that SecurityInfo flags for mixed content are reflected in the
// SecurityStyleExplanations produced by GetSecurityStyle.
TEST(SecurityStateContentUtilsTest, GetSecurityStyleForMixedContent) {
  content::SecurityStyleExplanations explanations;
  security_state::SecurityInfo security_info;
  security_info.cert_status = 0;
  security_info.scheme_is_cryptographic = true;

  security_info.contained_mixed_form = true;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_TRUE(explanations.contained_mixed_form);
  EXPECT_FALSE(explanations.ran_mixed_content);
  EXPECT_FALSE(explanations.displayed_mixed_content);

  security_info.contained_mixed_form = false;
  security_info.mixed_content_status = security_state::CONTENT_STATUS_DISPLAYED;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_FALSE(explanations.contained_mixed_form);
  EXPECT_TRUE(explanations.displayed_mixed_content);
}

// Tests that malicious safe browsing data in SecurityInfo triggers an
// appropriate summary in SecurityStyleExplanations.
TEST(SecurityStateContentUtilsTest, GetSecurityStyleForSafeBrowsing) {
  content::SecurityStyleExplanations explanations;
  security_state::SecurityInfo security_info;
  security_info.cert_status = 0;
  security_info.scheme_is_cryptographic = true;
  security_info.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_MALWARE;

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  GetSecurityStyle(security_info, &explanations);
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
  security_state::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  security_info.scheme_is_cryptographic = true;
  net::SSLConnectionStatusSetCipherSuite(
      0xcca8 /* TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 */,
      &security_info.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &security_info.connection_status);
  security_info.key_exchange_group = 29;  // X25519

  std::string connection_title =
      l10n_util::GetStringUTF8(IDS_SSL_CONNECTION_TITLE);

  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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
  security_info.key_exchange_group = 0;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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
  net::SSLConnectionStatusSetCipherSuite(0x1301 /* TLS_AES_128_GCM_SHA256 */,
                                         &security_info.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_3,
                                     &security_info.connection_status);
  security_info.key_exchange_group = 29;  // X25519
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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

void UpdateObsoleteSSLStatus(security_state::SecurityInfo* info) {
  info->obsolete_ssl_status = net::ObsoleteSSLStatus(
      info->connection_status, info->peer_signature_algorithm);
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
  security_state::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  security_info.scheme_is_cryptographic = true;
  net::SSLConnectionStatusSetCipherSuite(
      0xc013 /* TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA */,
      &security_info.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &security_info.connection_status);
  security_info.key_exchange_group = 29;  // X25519
  UpdateObsoleteSSLStatus(&security_info);
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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
  security_info.peer_signature_algorithm = 0x0201;  // rsa_pkcs1_sha1
  UpdateObsoleteSSLStatus(&security_info);
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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
  security_info.peer_signature_algorithm = 0;  // TLS 1.0 doesn't negotiate a
                                               // signature algorithm.
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1,
                                     &security_info.connection_status);
  UpdateObsoleteSSLStatus(&security_info);
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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
      &security_info.connection_status);
  UpdateObsoleteSSLStatus(&security_info);
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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
      &security_info.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &security_info.connection_status);
  security_info.peer_signature_algorithm = 0x0804;  // rsa_pss_rsae_sha256
  UpdateObsoleteSSLStatus(&security_info);
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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
      &security_info.connection_status);
  security_info.peer_signature_algorithm = 0x0201;  // rsa_pkcs1_sha1
  UpdateObsoleteSSLStatus(&security_info);
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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
  security_state::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  security_info.scheme_is_cryptographic = true;
  net::SSLConnectionStatusSetCipherSuite(
      0xcca8 /* TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 */,
      &security_info.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &security_info.connection_status);
  security_info.key_exchange_group = 29;  // X25519

  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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
  security_state::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  security_info.scheme_is_cryptographic = true;
  net::SSLConnectionStatusSetCipherSuite(
      0xcca8 /* TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 */,
      &security_info.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &security_info.connection_status);
  security_info.key_exchange_group = 29;  // X25519

  std::string content_title =
      l10n_util::GetStringUTF8(IDS_RESOURCE_SECURITY_TITLE);

  security_info.mixed_content_status =
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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

  security_info.mixed_content_status = security_state::CONTENT_STATUS_DISPLAYED;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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

  security_info.mixed_content_status = security_state::CONTENT_STATUS_RAN;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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

  security_info.contained_mixed_form = true;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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
  security_state::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  security_info.scheme_is_cryptographic = true;
  net::SSLConnectionStatusSetCipherSuite(
      0xcca8 /* TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 */,
      &security_info.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &security_info.connection_status);
  security_info.key_exchange_group = 29;  // X25519

  std::string content_title =
      l10n_util::GetStringUTF8(IDS_RESOURCE_SECURITY_TITLE);

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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

  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_RAN;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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
  security_state::SecurityInfo security_info;
  security_info.cert_status = net::CERT_STATUS_UNABLE_TO_CHECK_REVOCATION;
  security_info.scheme_is_cryptographic = true;
  net::SSLConnectionStatusSetCipherSuite(
      0xcca8 /* TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 */,
      &security_info.connection_status);
  net::SSLConnectionStatusSetVersion(net::SSL_CONNECTION_VERSION_TLS1_2,
                                     &security_info.connection_status);
  security_info.key_exchange_group = 29;  // X25519

  std::string content_title =
      l10n_util::GetStringUTF8(IDS_RESOURCE_SECURITY_TITLE);

  security_info.mixed_content_status =
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  {
    content::SecurityStyleExplanations explanations;
    GetSecurityStyle(security_info, &explanations);
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

// Tests that a security level of HTTP_SHOW_WARNING produces
// blink::WebSecurityStyleNeutral and an explanation if appropriate.
TEST(SecurityStateContentUtilsTest, HTTPWarning) {
  security_state::SecurityInfo security_info;
  content::SecurityStyleExplanations explanations;
  security_info.security_level = security_state::HTTP_SHOW_WARNING;
  blink::WebSecurityStyle security_style =
      GetSecurityStyle(security_info, &explanations);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, security_style);
  // Verify no explanation was shown, because Form Not Secure was not triggered.
  EXPECT_EQ(0u, explanations.neutral_explanations.size());

  explanations.neutral_explanations.clear();
  security_info.insecure_input_events.credit_card_field_edited = true;
  security_style = GetSecurityStyle(security_info, &explanations);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, security_style);
  // Verify one explanation was shown, because Form Not Secure was triggered.
  EXPECT_EQ(1u, explanations.neutral_explanations.size());

  // Check that when both password and credit card fields get displayed, only
  // one explanation is added.
  explanations.neutral_explanations.clear();
  security_info.insecure_input_events.credit_card_field_edited = true;
  security_info.insecure_input_events.password_field_shown = true;
  security_style = GetSecurityStyle(security_info, &explanations);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, security_style);
  // Verify only one explanation was shown when Form Not Secure is triggered.
  EXPECT_EQ(1u, explanations.neutral_explanations.size());

  // Verify that two explanations are shown when the Incognito and
  // FormNotSecure flags are both set.
  explanations.neutral_explanations.clear();
  security_info.insecure_input_events.credit_card_field_edited = true;
  security_info.insecure_input_events.password_field_shown = false;
  security_info.incognito_downgraded_security_level = true;
  security_style = GetSecurityStyle(security_info, &explanations);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, security_style);
  EXPECT_EQ(2u, explanations.neutral_explanations.size());

  // Verify that three explanations are shown when the Incognito, FormNotSecure,
  // and Insecure Field Edit flags are all set.
  explanations.neutral_explanations.clear();
  security_info.insecure_input_events.credit_card_field_edited = true;
  security_info.insecure_input_events.password_field_shown = false;
  security_info.incognito_downgraded_security_level = true;
  security_info.field_edit_downgraded_security_level = true;
  security_style = GetSecurityStyle(security_info, &explanations);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, security_style);
  EXPECT_EQ(3u, explanations.neutral_explanations.size());

  // Verify that one explanation is shown when the Insecure Field Edit flags
  // alone is set.
  explanations.neutral_explanations.clear();
  security_info.insecure_input_events.credit_card_field_edited = false;
  security_info.insecure_input_events.password_field_shown = false;
  security_info.incognito_downgraded_security_level = false;
  security_info.field_edit_downgraded_security_level = true;
  security_style = GetSecurityStyle(security_info, &explanations);
  EXPECT_EQ(blink::kWebSecurityStyleNeutral, security_style);
  EXPECT_EQ(1u, explanations.neutral_explanations.size());
}

// Tests that an explanation is provided if a certificate is missing a
// subjectAltName extension containing a domain name or IP address.
TEST(SecurityStateContentUtilsTest, SubjectAltNameWarning) {
  security_state::SecurityInfo security_info;
  security_info.cert_status = 0;
  security_info.scheme_is_cryptographic = true;

  security_info.certificate = net::ImportCertFromFile(
      net::GetTestCertsDirectory(), "salesforce_com_test.pem");
  ASSERT_TRUE(security_info.certificate);

  content::SecurityStyleExplanations explanations;
  security_info.cert_missing_subject_alt_name = true;
  GetSecurityStyle(security_info, &explanations);
  // Verify that an explanation was shown for a missing subjectAltName.
  EXPECT_EQ(1u, explanations.insecure_explanations.size());

  explanations.insecure_explanations.clear();
  security_info.cert_missing_subject_alt_name = false;
  GetSecurityStyle(security_info, &explanations);
  // Verify that no explanation is shown if the subjectAltName is present.
  EXPECT_EQ(0u, explanations.insecure_explanations.size());
}

// Tests that malicious safe browsing data in SecurityInfo causes an insecure
// explanation to be set.
TEST(SecurityStateContentUtilsTest, SafeBrowsingExplanation) {
  security_state::SecurityInfo security_info;
  security_info.cert_status = 0;
  security_info.scheme_is_cryptographic = true;
  security_info.malicious_content_status =
      security_state::MALICIOUS_CONTENT_STATUS_MALWARE;
  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_NONE;
  content::SecurityStyleExplanations explanations;
  GetSecurityStyle(security_info, &explanations);
  EXPECT_EQ(1u, explanations.insecure_explanations.size());
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
  security_state::SecurityInfo security_info;
  security_info.cert_status = 0;
  security_info.scheme_is_cryptographic = true;
  security_info.content_with_cert_errors_status =
      security_state::CONTENT_STATUS_NONE;

  // Check that an info explanation is provided if the certificate is expiring
  // in less than 48 hours.
  content::SecurityStyleExplanations explanations;
  security_info.certificate = scoped_refptr<net::X509Certificate>(
      CreateFakeCert(base::TimeDelta::FromHours(30)));
  ASSERT_TRUE(security_info.certificate);
  GetSecurityStyle(security_info, &explanations);
  EXPECT_EQ(1u, explanations.info_explanations.size());

  // Check that no explanation is set if the certificate is expiring in more
  // than 48 hours.
  explanations.info_explanations.clear();
  security_info.certificate = scoped_refptr<net::X509Certificate>(
      CreateFakeCert(base::TimeDelta::FromHours(72)));
  ASSERT_TRUE(security_info.certificate);
  GetSecurityStyle(security_info, &explanations);
  EXPECT_EQ(0u, explanations.info_explanations.size());

  // Check that no explanation is set if the certificate has already expired.
  explanations.info_explanations.clear();
  security_info.certificate = scoped_refptr<net::X509Certificate>(
      CreateFakeCert(base::TimeDelta::FromHours(-10)));
  ASSERT_TRUE(security_info.certificate);
  GetSecurityStyle(security_info, &explanations);
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
