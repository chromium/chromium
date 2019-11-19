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

// For nonsecure pages, returns a SecurityLevel based on the
// provided information and the kMarkHttpAsFeature field trial.
SecurityLevel GetSecurityLevelForNonSecureFieldTrial(
    bool is_error_page,
    const InsecureInputEventData& input_events) {
  if (base::FeatureList::IsEnabled(features::kMarkHttpAsFeature)) {
    std::string parameter = base::GetFieldTrialParamValueByFeature(
        features::kMarkHttpAsFeature,
        features::kMarkHttpAsFeatureParameterName);

    if (parameter == features::kMarkHttpAsParameterDangerous) {
      return DANGEROUS;
    }
    if (parameter == features::kMarkHttpAsParameterDangerWarning) {
      return WARNING;
    }
  }

  // Default to dangerous on editing form fields and otherwise
  // warning.
  return input_events.insecure_field_edited ? DANGEROUS : WARNING;
}

std::string GetHistogramSuffixForSecurityLevel(
    security_state::SecurityLevel level) {
  switch (level) {
    case EV_SECURE:
      return "EV_SECURE";
    case SECURE:
      return "SECURE";
    case NONE:
      return "NONE";
    case WARNING:
      return "WARNING";
    case SECURE_WITH_POLICY_INSTALLED_CERT:
      return "SECURE_WITH_POLICY_INSTALLED_CERT";
    case DANGEROUS:
      return "DANGEROUS";
    default:
      return "OTHER";
  }
}

std::string GetHistogramSuffixForSafetyTipStatus(
    security_state::SafetyTipStatus safety_tip_status) {
  switch (safety_tip_status) {
    case security_state::SafetyTipStatus::kUnknown:
      return "SafetyTip_Unknown";
    case security_state::SafetyTipStatus::kNone:
      return "SafetyTip_None";
    case security_state::SafetyTipStatus::kBadReputation:
      return "SafetyTip_BadReputation";
    case security_state::SafetyTipStatus::kLookalike:
      return "SafetyTip_Lookalike";
    case security_state::SafetyTipStatus::kBadReputationIgnored:
      return "SafetyTip_BadReputationIgnored";
    case security_state::SafetyTipStatus::kLookalikeIgnored:
      return "SafetyTip_LookalikeIgnored";
    case security_state::SafetyTipStatus::kBadKeyword:
      NOTREACHED();
      return std::string();
  }
  NOTREACHED();
  return std::string();
}

// Returns whether to set the security level based on the safety tip status.
// Sets |level| to the right value if status should be set.
bool ShouldSetSecurityLevelFromSafetyTip(security_state::SafetyTipStatus status,
                                         SecurityLevel* level) {
  if (!base::FeatureList::IsEnabled(security_state::features::kSafetyTipUI)) {
    return false;
  }

  switch (status) {
    case security_state::SafetyTipStatus::kBadReputation:
      *level = security_state::NONE;
      return true;
    case security_state::SafetyTipStatus::kBadReputationIgnored:
    case security_state::SafetyTipStatus::kLookalike:
    case security_state::SafetyTipStatus::kLookalikeIgnored:
    case security_state::SafetyTipStatus::kBadKeyword:
      // TODO(crbug/1012982): Decide whether to degrade the indicator once the
      // UI lands.
    case security_state::SafetyTipStatus::kUnknown:
    case security_state::SafetyTipStatus::kNone:
      return false;
  }
  NOTREACHED();
  return false;
}

}  // namespace

SecurityLevel GetSecurityLevel(
    const VisibleSecurityState& visible_security_state,
    bool used_policy_installed_certificate,
    IsOriginSecureCallback is_origin_secure_callback) {
  // Override the connection security information if the website failed the
  // browser's malware checks.
  if (visible_security_state.malicious_content_status !=
      MALICIOUS_CONTENT_STATUS_NONE) {
    return DANGEROUS;
  }

  if (!visible_security_state.connection_info_initialized) {
    return NONE;
  }

  // Set the security level to DANGEROUS for major certificate errors.
  if (HasMajorCertificateError(visible_security_state)) {
    return DANGEROUS;
  }
  DCHECK(!net::IsCertStatusError(visible_security_state.cert_status));

  const GURL& url = visible_security_state.url;

  // data: URLs don't define a secure context, and are a vector for spoofing.
  // Likewise, ftp: URLs are always non-secure, and are uncommon enough that
  // we can treat them as such without significant user impact.
  //
  // Display a "Not secure" badge for all these URLs.
  if (url.SchemeIs(url::kDataScheme) || url.SchemeIs(url::kFtpScheme)) {
    return WARNING;
  }

  // Display DevTools pages as neutral since we can't be confident the page
  // is secure, but also don't want the "Not secure" badge.
  if (visible_security_state.is_devtools) {
    return NONE;
  }

  // Choose the appropriate security level for requests to HTTP and remaining
  // pseudo URLs (blob:, filesystem:). filesystem: is a standard scheme so does
  // not need to be explicitly listed here.
  // TODO(meacer): Remove special case for blob (crbug.com/684751).
  const bool is_cryptographic_with_certificate =
      visible_security_state.url.SchemeIsCryptographic() &&
      visible_security_state.certificate;
  if (!is_cryptographic_with_certificate) {
    if (!visible_security_state.is_error_page &&
        !is_origin_secure_callback.Run(url) &&
        (url.IsStandard() ||
         IsNonsecureBlobUrl(url, is_origin_secure_callback))) {
      return GetSecurityLevelForNonSecureFieldTrial(
          visible_security_state.is_error_page,
          visible_security_state.insecure_input_events);
    }
    return NONE;
  }

  // Downgrade the security level for active insecure subresources.
  if (visible_security_state.ran_mixed_content ||
      visible_security_state.ran_content_with_cert_errors) {
    return kRanInsecureContentLevel;
  }

  // Downgrade the security level for pages loaded over legacy TLS versions.
  if (base::FeatureList::IsEnabled(
          security_state::features::kLegacyTLSWarnings) &&
      visible_security_state.connection_used_legacy_tls &&
      !visible_security_state.should_suppress_legacy_tls_warning) {
    return WARNING;
  }

  // Downgrade the security level for pages that trigger a Safety Tip.
  SecurityLevel safety_tip_level;
  if (ShouldSetSecurityLevelFromSafetyTip(
          visible_security_state.safety_tip_info.status, &safety_tip_level)) {
    return safety_tip_level;
  }

  // In most cases, SHA1 use is treated as a certificate error, in which case
  // DANGEROUS will have been returned above. If SHA1 was permitted by policy,
  // downgrade the security level to Neutral.
  if (IsSHA1InChain(visible_security_state)) {
    return NONE;
  }

  // Active mixed content is handled above.
  DCHECK(!visible_security_state.ran_mixed_content);
  DCHECK(!visible_security_state.ran_content_with_cert_errors);

  if (visible_security_state.contained_mixed_form ||
      visible_security_state.displayed_mixed_content ||
      visible_security_state.displayed_content_with_cert_errors) {
    return kDisplayedInsecureContentLevel;
  }

  if (visible_security_state.is_view_source) {
    return NONE;
  }

  // Any prior observation of a policy-installed cert is a strong indicator
  // of a MITM being present (the enterprise), so a "secure-but-inspected"
  // security level is returned.
  if (used_policy_installed_certificate) {
    return SECURE_WITH_POLICY_INSTALLED_CERT;
  }

  if ((visible_security_state.cert_status & net::CERT_STATUS_IS_EV) &&
      visible_security_state.certificate) {
    return EV_SECURE;
  }
  return SECURE;
}

bool HasMajorCertificateError(
    const VisibleSecurityState& visible_security_state) {
  if (!visible_security_state.connection_info_initialized)
    return false;

  const bool is_cryptographic_with_certificate =
      visible_security_state.url.SchemeIsCryptographic() &&
      visible_security_state.certificate;

  const bool is_major_cert_error =
      net::IsCertStatusError(visible_security_state.cert_status);

  return is_cryptographic_with_certificate && is_major_cert_error;
}

VisibleSecurityState::VisibleSecurityState()
    : malicious_content_status(MALICIOUS_CONTENT_STATUS_NONE),
      connection_info_initialized(false),
      cert_status(0),
      connection_status(0),
      key_exchange_group(0),
      peer_signature_algorithm(0),
      displayed_mixed_content(false),
      contained_mixed_form(false),
      ran_mixed_content(false),
      displayed_content_with_cert_errors(false),
      ran_content_with_cert_errors(false),
      pkp_bypassed(false),
      is_error_page(false),
      is_view_source(false),
      is_devtools(false),
      connection_used_legacy_tls(false),
      should_suppress_legacy_tls_warning(false) {}

VisibleSecurityState::VisibleSecurityState(const VisibleSecurityState& other) =
    default;
VisibleSecurityState& VisibleSecurityState::operator=(
    const VisibleSecurityState& other) = default;

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

std::string GetSecurityLevelHistogramName(
    const std::string& prefix,
    security_state::SecurityLevel level) {
  return prefix + "." + GetHistogramSuffixForSecurityLevel(level);
}

std::string GetSafetyTipHistogramName(const std::string& prefix,
                                      SafetyTipStatus safety_tip_status) {
  return prefix + "." + GetHistogramSuffixForSafetyTipStatus(safety_tip_status);
}

bool GetLegacyTLSWarningStatus(
    const VisibleSecurityState& visible_security_state) {
  return visible_security_state.connection_used_legacy_tls &&
         !visible_security_state.should_suppress_legacy_tls_warning;
}

std::string GetLegacyTLSHistogramName(
    const std::string& prefix,
    const VisibleSecurityState& visible_security_state) {
  if (GetLegacyTLSWarningStatus(visible_security_state)) {
    return prefix + "." + "LegacyTLS_Triggered";
  } else {
    return prefix + "." + "LegacyTLS_NotTriggered";
  }
}

bool IsSHA1InChain(const VisibleSecurityState& visible_security_state) {
  return visible_security_state.certificate &&
         (visible_security_state.cert_status &
          net::CERT_STATUS_SHA1_SIGNATURE_PRESENT);
}

// As an experiment, the info icon should be downgraded to a grey triangle for
// non-secure connections (crbug.com/997972). This method helps distinguish
// between cases where the NONE and WARNING security levels should map to
// neutral (info) or insecure (triangle) for styling purposes.
//
// TODO(crbug.com/1015626): Clean this up once the experiment is fully
// launched and security states refactored.
bool ShouldDowngradeNeutralStyling(
    security_state::SecurityLevel security_level,
    GURL url,
    IsOriginSecureCallback is_origin_secure_callback) {
  // This method is only relevant if the info icon is shown, which only occurs
  // for NONE and WARNING security states.
  if (security_level != security_state::NONE &&
      security_level != security_state::WARNING) {
    return false;
  }

  // The state should not be downgraded unless the grey triangle experiment
  // is enabled.
  bool http_danger_warning_enabled = false;
  if (base::FeatureList::IsEnabled(features::kMarkHttpAsFeature)) {
    std::string parameter = base::GetFieldTrialParamValueByFeature(
        features::kMarkHttpAsFeature,
        features::kMarkHttpAsFeatureParameterName);
    if (parameter ==
        security_state::features::kMarkHttpAsParameterDangerWarning) {
      http_danger_warning_enabled = true;
    }
  }
  if (!http_danger_warning_enabled)
    return false;

  bool scheme_is_cryptographic = security_state::IsSchemeCryptographic(url);
  bool origin_is_secure = scheme_is_cryptographic;
  if (!scheme_is_cryptographic)
    origin_is_secure = is_origin_secure_callback.Run(url);

  // The grey danger triangle should be shown on HTTPS pages with passive
  // mixed content. These pages currently use the NONE security state, but
  // this is undergoing a refactor.
  if (security_level == security_state::NONE && scheme_is_cryptographic)
    return true;

  // The info icon should be used on non-HTTPS secure origins, but other
  // WARNING states should use they grey danger triangle.
  if (security_level == security_state::WARNING && !origin_is_secure)
    return true;

  return false;
}

}  // namespace security_state
