// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/content/content_utils.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/security_state/content/ssl_status_input_event_data.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/security_style_explanation.h"
#include "content/public/browser/security_style_explanations.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_errors.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"
#include "third_party/blink/public/platform/web_mixed_content_context_type.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"
#include "ui/base/l10n/l10n_util.h"

namespace security_state {

namespace {

// Note: This is a lossy operation. Not all of the policies that can be
// expressed by a SecurityLevel can be expressed by a blink::WebSecurityStyle.
blink::WebSecurityStyle SecurityLevelToSecurityStyle(
    security_state::SecurityLevel security_level) {
  switch (security_level) {
    case security_state::NONE:
    case security_state::HTTP_SHOW_WARNING:
      return blink::kWebSecurityStyleNeutral;
    case security_state::SECURE_WITH_POLICY_INSTALLED_CERT:
    case security_state::EV_SECURE:
    case security_state::SECURE:
      return blink::kWebSecurityStyleSecure;
    case security_state::DANGEROUS:
      return blink::kWebSecurityStyleInsecure;
    case security_state::SECURITY_LEVEL_COUNT:
      NOTREACHED();
      return blink::kWebSecurityStyleNeutral;
  }

  NOTREACHED();
  return blink::kWebSecurityStyleUnknown;
}

void ExplainHTTPSecurity(
    const security_state::SecurityInfo& security_info,
    content::SecurityStyleExplanations* security_style_explanations) {
  if (security_info.security_level != security_state::HTTP_SHOW_WARNING)
    return;

  if (security_info.field_edit_downgraded_security_level) {
    security_style_explanations->neutral_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_EDITED_NONSECURE),
            l10n_util::GetStringUTF8(IDS_EDITED_NONSECURE_DESCRIPTION)));
  }
  if (security_info.insecure_input_events.password_field_shown ||
      security_info.insecure_input_events.credit_card_field_edited) {
    security_style_explanations->neutral_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_PRIVATE_USER_DATA_INPUT),
            l10n_util::GetStringUTF8(IDS_PRIVATE_USER_DATA_INPUT_DESCRIPTION)));
  }
  if (security_info.incognito_downgraded_security_level) {
    security_style_explanations->neutral_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_INCOGNITO_NONSECURE),
            l10n_util::GetStringUTF8(IDS_INCOGNITO_NONSECURE_DESCRIPTION)));
  }
}

void ExplainSafeBrowsingSecurity(
    const security_state::SecurityInfo& security_info,
    content::SecurityStyleExplanations* security_style_explanations) {
  if (security_info.malicious_content_status ==
      security_state::MALICIOUS_CONTENT_STATUS_NONE) {
    return;
  }
  // Override the main summary for the page.
  security_style_explanations->summary =
      l10n_util::GetStringUTF8(IDS_SAFEBROWSING_WARNING);
  // Add a bullet describing the issue.
  content::SecurityStyleExplanation explanation(
      l10n_util::GetStringUTF8(IDS_SAFEBROWSING_WARNING_SUMMARY),
      l10n_util::GetStringUTF8(IDS_SAFEBROWSING_WARNING_DESCRIPTION));
  security_style_explanations->insecure_explanations.push_back(explanation);
}

void ExplainCertificateSecurity(
    const security_state::SecurityInfo& security_info,
    content::SecurityStyleExplanations* security_style_explanations) {
  if (security_info.sha1_in_chain) {
    content::SecurityStyleExplanation explanation(
        l10n_util::GetStringUTF8(IDS_CERTIFICATE_TITLE),
        l10n_util::GetStringUTF8(IDS_SHA1),
        l10n_util::GetStringUTF8(IDS_SHA1_DESCRIPTION),
        security_info.certificate,
        blink::WebMixedContentContextType::kNotMixedContent);
    // The impact of SHA1 on the certificate status depends on
    // the EnableSHA1ForLocalAnchors policy.
    if (security_info.cert_status & net::CERT_STATUS_WEAK_SIGNATURE_ALGORITHM) {
      security_style_explanations->insecure_explanations.push_back(explanation);
    } else {
      security_style_explanations->neutral_explanations.push_back(explanation);
    }
  }

  if (security_info.cert_missing_subject_alt_name) {
    security_style_explanations->insecure_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_CERTIFICATE_TITLE),
            l10n_util::GetStringUTF8(IDS_SUBJECT_ALT_NAME_MISSING),
            l10n_util::GetStringUTF8(IDS_SUBJECT_ALT_NAME_MISSING_DESCRIPTION),
            security_info.certificate,
            blink::WebMixedContentContextType::kNotMixedContent));
  }

  bool is_cert_status_error = net::IsCertStatusError(security_info.cert_status);
  bool is_cert_status_minor_error =
      net::IsCertStatusMinorError(security_info.cert_status);

  if (is_cert_status_error) {
    base::string16 error_string = base::UTF8ToUTF16(net::ErrorToString(
        net::MapCertStatusToNetError(security_info.cert_status)));

    content::SecurityStyleExplanation explanation(
        l10n_util::GetStringUTF8(IDS_CERTIFICATE_TITLE),
        l10n_util::GetStringUTF8(IDS_CERTIFICATE_CHAIN_ERROR),
        l10n_util::GetStringFUTF8(
            IDS_CERTIFICATE_CHAIN_ERROR_DESCRIPTION_FORMAT, error_string),
        security_info.certificate,
        blink::WebMixedContentContextType::kNotMixedContent);

    if (is_cert_status_minor_error) {
      security_style_explanations->neutral_explanations.push_back(explanation);
    } else {
      security_style_explanations->insecure_explanations.push_back(explanation);
    }
  } else {
    // If the certificate does not have errors and is not using SHA1, then add
    // an explanation that the certificate is valid.

    base::string16 issuer_name;
    if (security_info.certificate) {
      // This results in the empty string if there is no relevant display name.
      issuer_name = base::UTF8ToUTF16(
          security_info.certificate->issuer().GetDisplayName());
    } else {
      issuer_name = base::string16();
    }
    if (issuer_name.empty()) {
      issuer_name.assign(
          l10n_util::GetStringUTF16(IDS_PAGE_INFO_SECURITY_TAB_UNKNOWN_PARTY));
    }

    if (!security_info.sha1_in_chain) {
      security_style_explanations->secure_explanations.push_back(
          content::SecurityStyleExplanation(
              l10n_util::GetStringUTF8(IDS_CERTIFICATE_TITLE),
              l10n_util::GetStringUTF8(IDS_VALID_SERVER_CERTIFICATE),
              l10n_util::GetStringFUTF8(
                  IDS_VALID_SERVER_CERTIFICATE_DESCRIPTION, issuer_name),
              security_info.certificate,
              blink::WebMixedContentContextType::kNotMixedContent));
    }
  }

  security_style_explanations->pkp_bypassed = security_info.pkp_bypassed;
  if (security_info.pkp_bypassed) {
    security_style_explanations->info_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_CERTIFICATE_TITLE),
            l10n_util::GetStringUTF8(IDS_PRIVATE_KEY_PINNING_BYPASSED),
            l10n_util::GetStringUTF8(
                IDS_PRIVATE_KEY_PINNING_BYPASSED_DESCRIPTION)));
  }

  if (security_info.certificate &&
      !security_info.certificate->valid_expiry().is_null() &&
      (security_info.certificate->valid_expiry() - base::Time::Now())
              .InHours() < 48 &&
      (security_info.certificate->valid_expiry() > base::Time::Now())) {
    security_style_explanations->info_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_CERTIFICATE_EXPIRING_SOON),
            l10n_util::GetStringUTF8(
                IDS_CERTIFICATE_EXPIRING_SOON_DESCRIPTION)));
  }
}

void ExplainConnectionSecurity(
    const security_state::SecurityInfo& security_info,
    content::SecurityStyleExplanations* security_style_explanations) {
  // Avoid showing TLS details when we couldn't even establish a TLS connection
  // (e.g. for net errors) or if there was no real connection (some tests). We
  // check the |connection_status| to see if there was a connection.
  if (security_info.connection_status == 0) {
    return;
  }

  int ssl_version =
      net::SSLConnectionStatusToVersion(security_info.connection_status);
  const char* protocol;
  net::SSLVersionToString(&protocol, ssl_version);
  const char* key_exchange;
  const char* cipher;
  const char* mac;
  bool is_aead;
  bool is_tls13;
  uint16_t cipher_suite =
      net::SSLConnectionStatusToCipherSuite(security_info.connection_status);
  net::SSLCipherSuiteToStrings(&key_exchange, &cipher, &mac, &is_aead,
                               &is_tls13, cipher_suite);
  const base::string16 protocol_name = base::ASCIIToUTF16(protocol);
  const base::string16 cipher_name = base::ASCIIToUTF16(cipher);
  const base::string16 cipher_full_name =
      (mac == nullptr) ? cipher_name
                       : l10n_util::GetStringFUTF16(IDS_CIPHER_WITH_MAC,
                                                    base::ASCIIToUTF16(cipher),
                                                    base::ASCIIToUTF16(mac));

  // Include the key exchange group (previously known as curve) if specified.
  base::string16 key_exchange_name;
  if (is_tls13) {
    key_exchange_name = base::ASCIIToUTF16(
        SSL_get_curve_name(security_info.key_exchange_group));
  } else if (security_info.key_exchange_group != 0) {
    key_exchange_name = l10n_util::GetStringFUTF16(
        IDS_SSL_KEY_EXCHANGE_WITH_GROUP, base::ASCIIToUTF16(key_exchange),
        base::ASCIIToUTF16(
            SSL_get_curve_name(security_info.key_exchange_group)));
  } else {
    key_exchange_name = base::ASCIIToUTF16(key_exchange);
  }

  int status = security_info.obsolete_ssl_status;
  if (status == net::OBSOLETE_SSL_NONE) {
    security_style_explanations->secure_explanations.emplace_back(
        l10n_util::GetStringUTF8(IDS_SSL_CONNECTION_TITLE),
        l10n_util::GetStringUTF8(IDS_SECURE_SSL_SUMMARY),
        l10n_util::GetStringFUTF8(IDS_SSL_DESCRIPTION, protocol_name,
                                  key_exchange_name, cipher_full_name));
    return;
  }

  std::vector<std::string> recommendations;
  if (status & net::OBSOLETE_SSL_MASK_PROTOCOL) {
    recommendations.push_back(
        l10n_util::GetStringFUTF8(IDS_SSL_RECOMMEND_PROTOCOL, protocol_name));
  }
  if (status & net::OBSOLETE_SSL_MASK_KEY_EXCHANGE) {
    recommendations.push_back(
        l10n_util::GetStringUTF8(IDS_SSL_RECOMMEND_KEY_EXCHANGE));
  }
  if (status & net::OBSOLETE_SSL_MASK_CIPHER) {
    // The problems with obsolete encryption come from the cipher portion rather
    // than the MAC, so use the shorter |cipher_name| rather than
    // |cipher_full_name|.
    recommendations.push_back(
        l10n_util::GetStringFUTF8(IDS_SSL_RECOMMEND_CIPHER, cipher_name));
  }
  if (status & net::OBSOLETE_SSL_MASK_SIGNATURE) {
    recommendations.push_back(
        l10n_util::GetStringUTF8(IDS_SSL_RECOMMEND_SIGNATURE));
  }

  security_style_explanations->info_explanations.emplace_back(
      l10n_util::GetStringUTF8(IDS_SSL_CONNECTION_TITLE),
      l10n_util::GetStringUTF8(IDS_OBSOLETE_SSL_SUMMARY),
      l10n_util::GetStringFUTF8(IDS_SSL_DESCRIPTION, protocol_name,
                                key_exchange_name, cipher_full_name),
      std::move(recommendations));
}

void ExplainContentSecurity(
    const security_state::SecurityInfo& security_info,
    content::SecurityStyleExplanations* security_style_explanations) {
  security_style_explanations->ran_insecure_content_style =
      SecurityLevelToSecurityStyle(security_state::kRanInsecureContentLevel);
  security_style_explanations->displayed_insecure_content_style =
      SecurityLevelToSecurityStyle(
          security_state::kDisplayedInsecureContentLevel);

  // Add the secure explanation unless there is an issue.
  bool add_secure_explanation = true;

  security_style_explanations->ran_mixed_content =
      security_info.mixed_content_status ==
          security_state::CONTENT_STATUS_RAN ||
      security_info.mixed_content_status ==
          security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  if (security_style_explanations->ran_mixed_content) {
    add_secure_explanation = false;
    security_style_explanations->insecure_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_RESOURCE_SECURITY_TITLE),
            l10n_util::GetStringUTF8(IDS_MIXED_ACTIVE_CONTENT_SUMMARY),
            l10n_util::GetStringUTF8(IDS_MIXED_ACTIVE_CONTENT_DESCRIPTION),
            nullptr, blink::WebMixedContentContextType::kBlockable));
  }

  security_style_explanations->displayed_mixed_content =
      security_info.mixed_content_status ==
          security_state::CONTENT_STATUS_DISPLAYED ||
      security_info.mixed_content_status ==
          security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
  if (security_style_explanations->displayed_mixed_content) {
    add_secure_explanation = false;
    security_style_explanations->neutral_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_RESOURCE_SECURITY_TITLE),
            l10n_util::GetStringUTF8(IDS_MIXED_PASSIVE_CONTENT_SUMMARY),
            l10n_util::GetStringUTF8(IDS_MIXED_PASSIVE_CONTENT_DESCRIPTION),
            nullptr, blink::WebMixedContentContextType::kOptionallyBlockable));
  }

  security_style_explanations->contained_mixed_form =
      security_info.contained_mixed_form;
  if (security_style_explanations->contained_mixed_form) {
    add_secure_explanation = false;
    security_style_explanations->neutral_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_RESOURCE_SECURITY_TITLE),
            l10n_util::GetStringUTF8(IDS_NON_SECURE_FORM_SUMMARY),
            l10n_util::GetStringUTF8(IDS_NON_SECURE_FORM_DESCRIPTION)));
  }

  // If the main resource was loaded with no certificate errors or only minor
  // certificate errors, then record the presence of subresources with
  // certificate errors. Subresource certificate errors aren't recorded when the
  // main resource was loaded with major certificate errors because, in the
  // common case, these subresource certificate errors would be duplicative with
  // the main resource's error.
  bool is_cert_status_error = net::IsCertStatusError(security_info.cert_status);
  bool is_cert_status_minor_error =
      net::IsCertStatusMinorError(security_info.cert_status);
  if (!is_cert_status_error || is_cert_status_minor_error) {
    security_style_explanations->ran_content_with_cert_errors =
        security_info.content_with_cert_errors_status ==
            security_state::CONTENT_STATUS_RAN ||
        security_info.content_with_cert_errors_status ==
            security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
    if (security_style_explanations->ran_content_with_cert_errors) {
      add_secure_explanation = false;
      security_style_explanations->insecure_explanations.push_back(
          content::SecurityStyleExplanation(
              l10n_util::GetStringUTF8(IDS_RESOURCE_SECURITY_TITLE),
              l10n_util::GetStringUTF8(IDS_CERT_ERROR_ACTIVE_CONTENT_SUMMARY),
              l10n_util::GetStringUTF8(
                  IDS_CERT_ERROR_ACTIVE_CONTENT_DESCRIPTION)));
    }

    security_style_explanations->displayed_content_with_cert_errors =
        security_info.content_with_cert_errors_status ==
            security_state::CONTENT_STATUS_DISPLAYED ||
        security_info.content_with_cert_errors_status ==
            security_state::CONTENT_STATUS_DISPLAYED_AND_RAN;
    if (security_style_explanations->displayed_content_with_cert_errors) {
      add_secure_explanation = false;
      security_style_explanations->neutral_explanations.push_back(
          content::SecurityStyleExplanation(
              l10n_util::GetStringUTF8(IDS_RESOURCE_SECURITY_TITLE),
              l10n_util::GetStringUTF8(IDS_CERT_ERROR_PASSIVE_CONTENT_SUMMARY),
              l10n_util::GetStringUTF8(
                  IDS_CERT_ERROR_PASSIVE_CONTENT_DESCRIPTION)));
    }
  }

  if (add_secure_explanation) {
    DCHECK(security_info.scheme_is_cryptographic);
    security_style_explanations->secure_explanations.push_back(
        content::SecurityStyleExplanation(
            l10n_util::GetStringUTF8(IDS_RESOURCE_SECURITY_TITLE),
            l10n_util::GetStringUTF8(IDS_SECURE_RESOURCES_SUMMARY),
            l10n_util::GetStringUTF8(IDS_SECURE_RESOURCES_DESCRIPTION)));
  }
}

}  // namespace

std::unique_ptr<security_state::VisibleSecurityState> GetVisibleSecurityState(
    content::WebContents* web_contents) {
  auto state = std::make_unique<security_state::VisibleSecurityState>();

  content::NavigationEntry* entry =
      web_contents->GetController().GetVisibleEntry();
  if (!entry)
    return state;
  // Set fields that are not dependent on the connection info.
  state->is_error_page = entry->GetPageType() == content::PAGE_TYPE_ERROR;
  state->is_view_source =
      entry->GetVirtualURL().SchemeIs(content::kViewSourceScheme);
  state->url = entry->GetURL();

  if (!entry->GetSSL().initialized)
    return state;
  state->connection_info_initialized = true;
  const content::SSLStatus& ssl = entry->GetSSL();
  state->certificate = ssl.certificate;
  state->cert_status = ssl.cert_status;
  state->connection_status = ssl.connection_status;
  state->key_exchange_group = ssl.key_exchange_group;
  state->peer_signature_algorithm = ssl.peer_signature_algorithm;
  state->security_bits = ssl.security_bits;
  state->pkp_bypassed = ssl.pkp_bypassed;
  state->displayed_mixed_content =
      !!(ssl.content_status & content::SSLStatus::DISPLAYED_INSECURE_CONTENT);
  state->ran_mixed_content =
      !!(ssl.content_status & content::SSLStatus::RAN_INSECURE_CONTENT);
  state->displayed_content_with_cert_errors =
      !!(ssl.content_status &
         content::SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS);
  state->ran_content_with_cert_errors =
      !!(ssl.content_status & content::SSLStatus::RAN_CONTENT_WITH_CERT_ERRORS);
  state->contained_mixed_form =
      !!(ssl.content_status &
         content::SSLStatus::DISPLAYED_FORM_WITH_INSECURE_ACTION);

  SSLStatusInputEventData* input_events =
      static_cast<SSLStatusInputEventData*>(ssl.user_data.get());

  if (input_events)
    state->insecure_input_events = *input_events->input_events();

  return state;
}

blink::WebSecurityStyle GetSecurityStyle(
    const security_state::SecurityInfo& security_info,
    content::SecurityStyleExplanations* security_style_explanations) {
  const blink::WebSecurityStyle security_style =
      SecurityLevelToSecurityStyle(security_info.security_level);

  ExplainHTTPSecurity(security_info, security_style_explanations);
  ExplainSafeBrowsingSecurity(security_info, security_style_explanations);

  // Check if the page is HTTP; if so, no more explanations are needed. Note
  // that SecurityStyleUnauthenticated does not necessarily mean that
  // the page is loaded over HTTP, because the security style merely
  // represents how the embedder wishes to display the security state of
  // the page, and the embedder can choose to display HTTPS page as HTTP
  // if it wants to (for example, displaying deprecated crypto
  // algorithms with the same UI treatment as HTTP pages).
  security_style_explanations->scheme_is_cryptographic =
      security_info.scheme_is_cryptographic;
  if (!security_info.scheme_is_cryptographic) {
    return security_style;
  }

  ExplainCertificateSecurity(security_info, security_style_explanations);
  ExplainConnectionSecurity(security_info, security_style_explanations);
  ExplainContentSecurity(security_info, security_style_explanations);

  return security_style;
}

}  // namespace security_state
