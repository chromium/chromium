// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_STATE_CORE_SECURITY_STATE_H_
#define COMPONENTS_SECURITY_STATE_CORE_SECURITY_STATE_H_

#include <stdint.h>
#include <memory>
#include <string>

#include "base/feature_list.h"
#include "net/base/url_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "url/gurl.h"

// Provides helper methods and data types that are used to determine the
// high-level security information about a page or request.
//
// SecurityLevel is the main result, describing a page's or request's
// security state. It is computed by the platform-independent GetSecurityLevel()
// helper method, which receives platform-specific inputs from its callers in
// the form of a VisibleSecurityState struct.
namespace security_state {

// Describes the overall security state of the page.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// If you change this enum, you may need to update the UI icons in
// LocationBarModelImpl::GetVectorIcon and GetIconForSecurityState.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.security_state
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ConnectionSecurityLevel
enum SecurityLevel {
  // Neutral; neither positively secure nor insecure. Used for e.g. some types
  // of non-http/https URLs.
  NONE = 0,

  // HTTP, in a case where we want to show a visible warning about the page's
  // lack of security.
  //
  // The criteria used to classify pages as NONE vs. HTTP_SHOW_WARNING will
  // change over time. Eventually, NONE will be eliminated.
  // See https://crbug.com/647754.
  // DEPRECATED: Use WARNING instead in most cases.
  // HTTP_SHOW_WARNING = 1,

  // HTTPS with valid EV cert.
  // DEPRECATED: EV certs no longer receive special UI treatment.
  // See https://crbug.com/1006943.
  // EV_SECURE = 2,

  // HTTPS (non-EV) with valid cert.
  SECURE = 3,

  // HTTPS, but a certificate chain anchored to a root certificate installed
  // by the system administrator has been observed in this profile, suggesting
  // a MITM was present.
  //
  // Used only on ChromeOS, this status is unreached on other platforms.
  // TODO(crbug.com/40928765): make this work on all platforms when the
  // non-chromeos-specific policies are added.
  SECURE_WITH_POLICY_INSTALLED_CERT = 4,

  // Attempted HTTPS and failed, page not authenticated, HTTPS with
  // insecure active content on the page, malware, phishing, or any other
  // serious security issue that could be dangerous.
  DANGEROUS = 5,

  // Pages deemed insecure, where we should show a warning indicator. This
  // includes HTTP pages (previously these were HTTP_SHOW_WARNING) and cases
  // where we consider an HTTPS page to be insecure (e.g., legacy TLS versions).
  WARNING = 6,

  SECURITY_LEVEL_COUNT
};

// The ContentStatus enum is used to describe content on the page that
// has significantly different security properties than the main page
// load. Content can be passive content that is displayed (such as
// images) or active content that is run (such as scripts or iframes).
enum ContentStatus {
  CONTENT_STATUS_UNKNOWN,
  CONTENT_STATUS_NONE,
  CONTENT_STATUS_DISPLAYED,
  CONTENT_STATUS_RAN,
  CONTENT_STATUS_DISPLAYED_AND_RAN,
};

// Describes whether the page contains malicious resources such as
// malware or phishing attacks.
enum MaliciousContentStatus {
  MALICIOUS_CONTENT_STATUS_NONE,
  MALICIOUS_CONTENT_STATUS_MALWARE,
  MALICIOUS_CONTENT_STATUS_UNWANTED_SOFTWARE,
  MALICIOUS_CONTENT_STATUS_SOCIAL_ENGINEERING,
  MALICIOUS_CONTENT_STATUS_SAVED_PASSWORD_REUSE,
  MALICIOUS_CONTENT_STATUS_SIGNED_IN_SYNC_PASSWORD_REUSE,
  MALICIOUS_CONTENT_STATUS_SIGNED_IN_NON_SYNC_PASSWORD_REUSE,
  MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE,
  MALICIOUS_CONTENT_STATUS_BILLING,
  MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_WARN,
  MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_BLOCK,
};

// Describes whether the page triggers any safety tips or reputation
// warnings.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// Style note: this differs from other enums in this file to follow new
// histogram enum naming conventions
// (https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#usage).
enum class SafetyTipStatus {
  // Safety tip status is not applicable, e.g. there is no current navigation:
  kUnknown = 0,
  // The current page did not trigger any Safety Tip:
  kNone = 1,
  // The current page triggered a Safety Tip because it was bad reputation:
  // kBadReputation = 2, // no longer used, heuristic removed.
  // The current page triggered a Safety Tip because it had a lookalike URL:
  kLookalike = 3,
  // The current page triggered a Safety Tip because a suspicious keyword was
  // found in its hostname:
  // kBadKeyword = 4, // no longer used, heuristic removed.
  // The current page had bad reputation, but a Safety Tip was not shown since
  // it had been previously ignored by the user:
  // kBadReputationIgnored = 5, // no longer used, heuristic removed.
  // The current page had a lookalike URL, but a Safety Tip was not shown since
  // it had been previously ignored by the user:
  kLookalikeIgnored = 6,
  // Safety tip UI was ignored because of the lookalike's digital asset link
  // manifest matched the target's:
  // kDigitalAssetLinkMatch = 7, // no longer used, DAL checks removed.
  kMaxValue = kLookalikeIgnored,
};

// Information about the last safety tip shown in the UI. This is used in page
// info and security tab (in devtools) to give more information about the safety
// tip.
struct SafetyTipInfo {
  SafetyTipStatus status = SafetyTipStatus::kUnknown;
  // The URL the safety tip suggested ("Did you mean?"). Only filled in for
  // lookalike matches.
  GURL safe_url;
};

// Contains the security state relevant to computing the SecurityLevel
// for a page. This is the input to GetSecurityLevel().
struct VisibleSecurityState {
  VisibleSecurityState();
  VisibleSecurityState(const VisibleSecurityState& other);
  VisibleSecurityState& operator=(const VisibleSecurityState& other);
  ~VisibleSecurityState();

  GURL url;

  MaliciousContentStatus malicious_content_status;

  // What type of Safety Tip (if any) triggered on the page. Note that this
  // field will be set even if the Safety Tip UI was not actually shown due to
  // the feature being disabled (so that this field can be used to record
  // metrics independent of whether the UI actually showed).
  SafetyTipInfo safety_tip_info;

  // CONNECTION SECURITY FIELDS
  // Whether the connection security fields are initialized.
  bool connection_info_initialized;
  // The following fields contain information about the connection
  // used to load the page or request.
  scoped_refptr<net::X509Certificate> certificate;
  net::CertStatus cert_status;
  int connection_status;
  // The ID of the (EC)DH group used by the key exchange. The value is zero if
  // unknown (older cache entries may not store the value) or not applicable.
  uint16_t key_exchange_group;
  // The signature algorithm used by the peer in the TLS handshake, or zero if
  // unknown (older cache entries may not store the value) or not applicable.
  uint16_t peer_signature_algorithm;
  // True if the page displayed passive mixed content.
  bool displayed_mixed_content;
  // True if the secure page contained a form with a nonsecure target.
  bool contained_mixed_form;
  // True if the page ran active mixed content.
  bool ran_mixed_content;
  // True if the page displayed passive subresources with certificate errors.
  bool displayed_content_with_cert_errors;
  // True if the page ran active subresources with certificate errors.
  bool ran_content_with_cert_errors;
  // True if PKP was bypassed due to a local trust anchor.
  bool pkp_bypassed;
  // True if the page was an error page.
  // TODO(estark): this field is not populated on iOS. https://crbug.com/760647
  bool is_error_page;
  // True if the page is a view-source page.
  bool is_view_source;
  // True if the page is a devtools page.
  bool is_devtools;
  // True if the page is a reader mode page.
  bool is_reader_mode;
  // True if mixed forms should be treated as secure from the visible security
  // state perspective (for example, if a different warning is being shown for
  // them).
  bool should_treat_displayed_mixed_forms_as_secure;
  // True if the page was the result of an HTTPS-Only Mode upgrade.
  bool is_https_only_mode_upgraded;
};

// These security levels describe the treatment given to pages that
// display and run mixed content. They are used to coordinate the
// treatment of mixed content with other security UI elements.
constexpr SecurityLevel kDisplayedInsecureContentLevel = NONE;
constexpr SecurityLevel kDisplayedInsecureContentWarningLevel = WARNING;
constexpr SecurityLevel kRanInsecureContentLevel = DANGEROUS;

// Returns a SecurityLevel to describe the current page.
// |visible_security_state| contains the relevant security state.
// |used_policy_installed_certificate| indicates whether the page or request
// is known to be loaded with a certificate installed by the system admin.
SecurityLevel GetSecurityLevel(
    const VisibleSecurityState& visible_security_state,
    bool used_policy_installed_certificate);

// Returns true if the current page was loaded using a cryptographic protocol
// and its certificate has any major errors.
bool HasMajorCertificateError(
    const VisibleSecurityState& visible_security_state);

// Returns true for a valid |url| with a cryptographic scheme, e.g., HTTPS, WSS.
bool IsSchemeCryptographic(const GURL& url);

// Returns true for a valid |url| with localhost or file:// scheme origin.
bool IsOriginLocalhostOrFile(const GURL& url);

// Returns true if the page has a valid SSL certificate. Only SECURE and
// SECURE_WITH_POLICY_INSTALLED_CERT are considered valid.
bool IsSslCertificateValid(security_state::SecurityLevel security_level);

// Returns the given prefix suffixed with a dot and the current security level.
std::string GetSecurityLevelHistogramName(
    const std::string& prefix, security_state::SecurityLevel level);

// Returns the given prefix suffixed with a dot and the given Safety Tip status.
std::string GetSafetyTipHistogramName(const std::string& prefix,
                                      SafetyTipStatus safety_tip_status);

bool IsSHA1InChain(const VisibleSecurityState& visible_security_state);

}  // namespace security_state

#endif  // COMPONENTS_SECURITY_STATE_CORE_SECURITY_STATE_H_
