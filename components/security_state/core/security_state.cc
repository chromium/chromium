// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/core/security_state.h"

#include <stdint.h>
#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "components/security_state/core/features.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_connection_status_flags.h"

namespace security_state {

namespace {

// Returns true if |url| is a blob: URL and its path parses as a GURL with a
// nonsecure origin, and false otherwise. See
// https://url.spec.whatwg.org/#origin.
bool IsNonsecureBlobUrl(
    const GURL& url,
    const IsOriginSecureCallback& is_origin_secure_callback) {
  if (!url.SchemeIs(url::kBlobScheme))
    return false;
  GURL inner_url(url.path());
  return !is_origin_secure_callback.Run(inner_url);
}

// For nonsecure pages, sets |security_level| in |*security_info| based on the
// provided information and the kMarkHttpAsFeature field trial. Also sets the
// explanatory fields |incognito_downgraded_security_level| and
// |field_edit_downgraded_security_level|.
void SetSecurityLevelAndRelatedFieldsForNonSecureFieldTrial(
    bool is_incognito,
    bool is_error_page,
    const InsecureInputEventData& input_events,
    SecurityInfo* security_info) {
  if (base::FeatureList::IsEnabled(features::kMarkHttpAsFeature)) {
    std::string parameter = base::GetFieldTrialParamValueByFeature(
        features::kMarkHttpAsFeature,
        features::kMarkHttpAsFeatureParameterName);

    if (parameter == features::kMarkHttpAsParameterDangerous) {
      security_info->security_level = DANGEROUS;
      return;
    }

    if (parameter ==
        features::
            kMarkHttpAsParameterWarningAndDangerousOnPasswordsAndCreditCards) {
      security_info->security_level = (input_events.password_field_shown ||
                                       input_events.credit_card_field_edited)
                                          ? DANGEROUS
                                          : HTTP_SHOW_WARNING;
      return;
    }

    // By default, if the feature is enabled, show a warning on all http://
    // pages and mark as dangerous on form edits.
    security_info->security_level =
        input_events.insecure_field_edited ? DANGEROUS : HTTP_SHOW_WARNING;
    // Do not set |field_edit_downgraded_security_level| here because that
    // field is for specifically for when the security level was downgraded
    // from NONE to HTTP_SHOW_WARNING, not from HTTP_SHOW_WARNING to DANGEROUS
    // as in this case.
    return;
  }

  // No warning treatment is configured via field trial. Default to warning on
  // incognito or editing or sensitive form fields.
  security_info->security_level =
      (is_incognito || input_events.insecure_field_edited ||
       input_events.password_field_shown ||
       input_events.credit_card_field_edited)
          ? HTTP_SHOW_WARNING
          : NONE;
  security_info->incognito_downgraded_security_level =
      (is_incognito && !is_error_page &&
       security_info->security_level == HTTP_SHOW_WARNING);

  security_info->field_edit_downgraded_security_level =
      (security_info->security_level == HTTP_SHOW_WARNING &&
       input_events.insecure_field_edited);
}

ContentStatus GetContentStatus(bool displayed, bool ran) {
  if (ran && displayed)
    return CONTENT_STATUS_DISPLAYED_AND_RAN;
  if (ran)
    return CONTENT_STATUS_RAN;
  if (displayed)
    return CONTENT_STATUS_DISPLAYED;
  return CONTENT_STATUS_NONE;
}

// Sets |security_level| in |*security_info| based on the provided information,
// as well as the explanatory fields |incognito_downgraded_security_level| and
// |field_edit_downgraded_security_level|.
void SetSecurityLevelAndRelatedFields(
    const VisibleSecurityState& visible_security_state,
    bool used_policy_installed_certificate,
    const IsOriginSecureCallback& is_origin_secure_callback,
    bool sha1_in_chain,
    ContentStatus mixed_content_status,
    ContentStatus content_with_cert_errors_status,
    SecurityInfo* security_info) {
  DCHECK(visible_security_state.connection_info_initialized ||
         visible_security_state.malicious_content_status !=
             MALICIOUS_CONTENT_STATUS_NONE);

  // Initialize the related fields; they'll be set to true in
  // SetSecurityLevelAndRelatedFieldsForNonSecureFieldTrial() below if
  // necessary.
  security_info->incognito_downgraded_security_level = false;
  security_info->field_edit_downgraded_security_level = false;

  // Override the connection security information if the website failed the
  // browser's malware checks.
  if (visible_security_state.malicious_content_status !=
      MALICIOUS_CONTENT_STATUS_NONE) {
    security_info->security_level = DANGEROUS;
    return;
  }

  const GURL url = visible_security_state.url;

  const bool is_cryptographic_with_certificate =
      (url.SchemeIsCryptographic() && visible_security_state.certificate);

  // Set the security level to DANGEROUS for major certificate errors.
  if (is_cryptographic_with_certificate &&
      net::IsCertStatusError(visible_security_state.cert_status) &&
      !net::IsCertStatusMinorError(visible_security_state.cert_status)) {
    security_info->security_level = DANGEROUS;
    return;
  }

  // data: URLs don't define a secure context, and are a vector for spoofing.
  // Likewise, ftp: URLs are always non-secure, and are uncommon enough that
  // we can treat them as such without significant user impact.
  //
  // Display a "Not secure" badge for all these URLs, regardless of whether
  // they show a password or credit card field.
  if (url.SchemeIs(url::kDataScheme) || url.SchemeIs(url::kFtpScheme)) {
    security_info->security_level = SecurityLevel::HTTP_SHOW_WARNING;
    return;
  }

  // Choose the appropriate security level for requests to HTTP and remaining
  // pseudo URLs (blob:, filesystem:). filesystem: is a standard scheme so does
  // not need to be explicitly listed here.
  // TODO(meacer): Remove special case for blob (crbug.com/684751).
  if (!is_cryptographic_with_certificate) {
    if (!visible_security_state.is_error_page &&
        !is_origin_secure_callback.Run(url) &&
        (url.IsStandard() ||
         IsNonsecureBlobUrl(url, is_origin_secure_callback))) {
      SetSecurityLevelAndRelatedFieldsForNonSecureFieldTrial(
          visible_security_state.is_incognito,
          visible_security_state.is_error_page,
          visible_security_state.insecure_input_events, security_info);
      return;
    }
    security_info->security_level = NONE;
    return;
  }

  // Downgrade the security level for active insecure subresources.
  if (mixed_content_status == CONTENT_STATUS_RAN ||
      mixed_content_status == CONTENT_STATUS_DISPLAYED_AND_RAN ||
      content_with_cert_errors_status == CONTENT_STATUS_RAN ||
      content_with_cert_errors_status == CONTENT_STATUS_DISPLAYED_AND_RAN) {
    security_info->security_level = kRanInsecureContentLevel;
    return;
  }

  // In most cases, SHA1 use is treated as a certificate error, in which case
  // DANGEROUS will have been returned above. If SHA1 was permitted by policy,
  // downgrade the security level to Neutral.
  if (sha1_in_chain) {
    security_info->security_level = NONE;
    return;
  }

  // Active mixed content is handled above.
  DCHECK_NE(CONTENT_STATUS_RAN, mixed_content_status);
  DCHECK_NE(CONTENT_STATUS_DISPLAYED_AND_RAN, mixed_content_status);

  if (visible_security_state.contained_mixed_form ||
      mixed_content_status == CONTENT_STATUS_DISPLAYED ||
      content_with_cert_errors_status == CONTENT_STATUS_DISPLAYED) {
    security_info->security_level = kDisplayedInsecureContentLevel;
    return;
  }

  if (net::IsCertStatusError(visible_security_state.cert_status)) {
    // Major cert errors are handled above.
    DCHECK(net::IsCertStatusMinorError(visible_security_state.cert_status));
    security_info->security_level = NONE;
    return;
  }

  if (visible_security_state.is_view_source) {
    security_info->security_level = NONE;
    return;
  }

  // Any prior observation of a policy-installed cert is a strong indicator
  // of a MITM being present (the enterprise), so a "secure-but-inspected"
  // security level is returned.
  if (used_policy_installed_certificate) {
    security_info->security_level = SECURE_WITH_POLICY_INSTALLED_CERT;
    return;
  }

  if ((visible_security_state.cert_status & net::CERT_STATUS_IS_EV) &&
      visible_security_state.certificate) {
    security_info->security_level = EV_SECURE;
    return;
  }
  security_info->security_level = SECURE;
}

void SecurityInfoForRequest(
    const VisibleSecurityState& visible_security_state,
    bool used_policy_installed_certificate,
    const IsOriginSecureCallback& is_origin_secure_callback,
    SecurityInfo* security_info) {
  if (!visible_security_state.connection_info_initialized) {
    *security_info = SecurityInfo();
    security_info->malicious_content_status =
        visible_security_state.malicious_content_status;
    if (security_info->malicious_content_status !=
        MALICIOUS_CONTENT_STATUS_NONE) {
      SetSecurityLevelAndRelatedFields(
          visible_security_state, used_policy_installed_certificate,
          is_origin_secure_callback, false, CONTENT_STATUS_UNKNOWN,
          CONTENT_STATUS_UNKNOWN, security_info);
    }
    return;
  }
  security_info->certificate = visible_security_state.certificate;

  security_info->sha1_in_chain = visible_security_state.certificate &&
                                 (visible_security_state.cert_status &
                                  net::CERT_STATUS_SHA1_SIGNATURE_PRESENT);
  security_info->mixed_content_status =
      GetContentStatus(visible_security_state.displayed_mixed_content,
                       visible_security_state.ran_mixed_content);
  security_info->content_with_cert_errors_status = GetContentStatus(
      visible_security_state.displayed_content_with_cert_errors,
      visible_security_state.ran_content_with_cert_errors);
  security_info->security_bits = visible_security_state.security_bits;
  security_info->connection_status = visible_security_state.connection_status;
  security_info->key_exchange_group = visible_security_state.key_exchange_group;
  security_info->peer_signature_algorithm =
      visible_security_state.peer_signature_algorithm;
  security_info->cert_status = visible_security_state.cert_status;
  security_info->scheme_is_cryptographic =
      visible_security_state.url.SchemeIsCryptographic();
  security_info->obsolete_ssl_status =
      net::ObsoleteSSLStatus(security_info->connection_status,
                             security_info->peer_signature_algorithm);
  security_info->pkp_bypassed = visible_security_state.pkp_bypassed;

  security_info->malicious_content_status =
      visible_security_state.malicious_content_status;

  security_info->cert_missing_subject_alt_name =
      visible_security_state.certificate &&
      !visible_security_state.certificate->GetSubjectAltName(nullptr, nullptr);

  security_info->contained_mixed_form =
      visible_security_state.contained_mixed_form;

  SetSecurityLevelAndRelatedFields(
      visible_security_state, used_policy_installed_certificate,
      is_origin_secure_callback, security_info->sha1_in_chain,
      security_info->mixed_content_status,
      security_info->content_with_cert_errors_status, security_info);

  security_info->insecure_input_events =
      visible_security_state.insecure_input_events;
}

}  // namespace

SecurityInfo::SecurityInfo()
    : security_level(NONE),
      malicious_content_status(MALICIOUS_CONTENT_STATUS_NONE),
      sha1_in_chain(false),
      mixed_content_status(CONTENT_STATUS_NONE),
      content_with_cert_errors_status(CONTENT_STATUS_NONE),
      scheme_is_cryptographic(false),
      cert_status(0),
      security_bits(-1),
      connection_status(0),
      key_exchange_group(0),
      peer_signature_algorithm(0),
      obsolete_ssl_status(net::OBSOLETE_SSL_NONE),
      pkp_bypassed(false),
      contained_mixed_form(false),
      cert_missing_subject_alt_name(false),
      incognito_downgraded_security_level(false),
      field_edit_downgraded_security_level(false) {}

SecurityInfo::~SecurityInfo() {}

void GetSecurityInfo(
    std::unique_ptr<VisibleSecurityState> visible_security_state,
    bool used_policy_installed_certificate,
    IsOriginSecureCallback is_origin_secure_callback,
    SecurityInfo* result) {
  SecurityInfoForRequest(*visible_security_state,
                         used_policy_installed_certificate,
                         is_origin_secure_callback, result);
}

VisibleSecurityState::VisibleSecurityState()
    : malicious_content_status(MALICIOUS_CONTENT_STATUS_NONE),
      connection_info_initialized(false),
      cert_status(0),
      connection_status(0),
      key_exchange_group(0),
      peer_signature_algorithm(0),
      security_bits(-1),
      displayed_mixed_content(false),
      contained_mixed_form(false),
      ran_mixed_content(false),
      displayed_content_with_cert_errors(false),
      ran_content_with_cert_errors(false),
      pkp_bypassed(false),
      is_incognito(false),
      is_error_page(false),
      is_view_source(false) {}

VisibleSecurityState::~VisibleSecurityState() {}

bool IsSchemeCryptographic(const GURL& url) {
  return url.is_valid() && url.SchemeIsCryptographic();
}

bool IsOriginLocalhostOrFile(const GURL& url) {
  return url.is_valid() && (net::IsLocalhost(url) || url.SchemeIsFile());
}

bool IsSslCertificateValid(SecurityLevel security_level) {
  return security_level == SECURE || security_level == EV_SECURE ||
         security_level == SECURE_WITH_POLICY_INSTALLED_CERT;
}

}  // namespace security_state
