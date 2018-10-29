// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_STATE_CORE_SECURITY_STATE_H_
#define COMPONENTS_SECURITY_STATE_CORE_SECURITY_STATE_H_

#include <stdint.h>
#include <memory>

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/macros.h"
#include "components/security_state/core/insecure_input_event_data.h"
#include "net/base/url_util.h"
#include "net/cert/cert_status_flags.h"
#include "net/cert/sct_status_flags.h"
#include "net/cert/x509_certificate.h"
#include "url/gurl.h"

// Provides helper methods and data types that are used to determine the
// high-level security information about a page or request.
//
// SecurityInfo is the main data structure, describing a page's or request's
// security state. It is computed by the platform-independent GetSecurityInfo()
// helper method, which receives platform-specific inputs from its callers in
// the form of a VisibleSecurityState struct.
namespace security_state {

// Describes the overall security state of the page.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// If you change this enum, you may need to update the UI icons in
// ToolbarModelImpl::GetVectorIcon and GetIconForSecurityState.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.components.security_state
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ConnectionSecurityLevel
enum SecurityLevel {
  // HTTP/no URL/HTTPS but with insecure passive content on the page.
  NONE = 0,

  // HTTP, in a case where we want to show a visible warning about the page's
  // lack of security.
  //
  // The criteria used to classify pages as NONE vs. HTTP_SHOW_WARNING will
  // change over time. Eventually, NONE will be eliminated.
  // See https://crbug.com/647754.
  HTTP_SHOW_WARNING = 1,

  // HTTPS with valid EV cert.
  EV_SECURE = 2,

  // HTTPS (non-EV) with valid cert.
  SECURE = 3,

  // HTTPS, but a certificate chain anchored to a root certificate installed
  // by the system administrator has been observed in this profile, suggesting
  // a MITM was present.
  //
  // Used only on ChromeOS, this status is unreached on other platforms.
  SECURE_WITH_POLICY_INSTALLED_CERT = 4,

  // Attempted HTTPS and failed, page not authenticated, HTTPS with
  // insecure active content on the page, malware, phishing, or any other
  // serious security issue that could be dangerous.
  DANGEROUS = 5,

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
  MALICIOUS_CONTENT_STATUS_SIGN_IN_PASSWORD_REUSE,
  MALICIOUS_CONTENT_STATUS_ENTERPRISE_PASSWORD_REUSE,
  MALICIOUS_CONTENT_STATUS_BILLING,
};

// Describes the security status of a page or request. This is the
// main data structure provided by this class. SecurityInfo contains a
// SecurityLevel (which
// is a single value describing the overall security state) along with
// information that a consumer might want to display in UI to explain or
// elaborate on the SecurityLevel.
struct SecurityInfo {
  SecurityInfo();
  ~SecurityInfo();
  SecurityLevel security_level;
  // Describes the nature of the page's malicious content, if any.
  MaliciousContentStatus malicious_content_status;
  // True if a SHA1 signature was observed anywhere in the certificate chain.
  bool sha1_in_chain;
  // |mixed_content_status| describes the presence of content that was
  // loaded over a nonsecure (HTTP) connection.
  ContentStatus mixed_content_status;
  // |content_with_cert_errors_status| describes the presence of
  // content that was loaded over an HTTPS connection with
  // certificate errors.
  ContentStatus content_with_cert_errors_status;
  bool scheme_is_cryptographic;
  net::CertStatus cert_status;
  scoped_refptr<net::X509Certificate> certificate;
  // The security strength, in bits, of the SSL cipher suite. In late
  // 2015, 128 is considered the minimum.
  //
  // 0 means the connection uses HTTPS but is not encrypted.  -1 means
  // the security strength is unknown or the connection does not use
  // HTTPS.
  int security_bits;
  // Information about the SSL connection, such as protocol and
  // ciphersuite. See ssl_connection_flags.h in net.
  int connection_status;
  // The ID of the (EC)DH group used by the key exchange. The value is zero if
  // unknown (older cache entries may not store the value) or not applicable.
  uint16_t key_exchange_group;
  // The signature algorithm used by the peer in the TLS handshake, or zero if
  // unknown (older cache entries may not store the value) or not applicable.
  uint16_t peer_signature_algorithm;
  // A mask that indicates which of the protocol version,
  // key exchange, or cipher for the connection is considered
  // obsolete. See net::ObsoleteSSLMask for specific mask values.
  int obsolete_ssl_status;
  // True if pinning was bypassed due to a local trust anchor.
  bool pkp_bypassed;
  // True if the secure page contained a form with a nonsecure target.
  bool contained_mixed_form;
  // True if the server's certificate does not contain a
  // subjectAltName extension with a domain name or IP address.
  bool cert_missing_subject_alt_name;
  // True if the |security_level| was downgraded to HTTP_SHOW_WARNING because
  // the page was loaded while Incognito.
  bool incognito_downgraded_security_level;
  // True if the |security_level| was downgraded to HTTP_SHOW_WARNING because
  // of a field edit recorded in |insecure_input_events|.
  bool field_edit_downgraded_security_level;
  // Contains information about input events that may impact the security
  // level of the page.
  InsecureInputEventData insecure_input_events;
};

// Contains the security state relevant to computing the SecurityInfo
// for a page. This is the input to GetSecurityInfo().
struct VisibleSecurityState {
  VisibleSecurityState();
  ~VisibleSecurityState();
  GURL url;

  MaliciousContentStatus malicious_content_status;

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
  int security_bits;
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
  // True if the page was displayed in an Incognito context.
  bool is_incognito;
  // True if the page was an error page.
  // TODO(estark): this field is not populated on iOS. https://crbug.com/760647
  bool is_error_page;
  // True if the page is a view-source page.
  bool is_view_source;
  // Contains information about input events that may impact the security
  // level of the page.
  InsecureInputEventData insecure_input_events;
};

// These security levels describe the treatment given to pages that
// display and run mixed content. They are used to coordinate the
// treatment of mixed content with other security UI elements.
constexpr SecurityLevel kDisplayedInsecureContentLevel = NONE;
constexpr SecurityLevel kRanInsecureContentLevel = DANGEROUS;

// Returns true if the given |url|'s origin should be considered secure.
using IsOriginSecureCallback = base::Callback<bool(const GURL& url)>;

// Populates |result| to describe the current page.
// |visible_security_state| contains the relevant security state.
// |used_policy_installed_certificate| indicates whether the page or request
// is known to be loaded with a certificate installed by the system admin.
// |is_origin_secure_callback| determines whether a URL's origin should be
// considered secure.
void GetSecurityInfo(
    std::unique_ptr<VisibleSecurityState> visible_security_state,
    bool used_policy_installed_certificate,
    IsOriginSecureCallback is_origin_secure_callback,
    SecurityInfo* result);

// Returns true if an experimental form warning UI about HTTP passwords
// and credit cards is enabled. This warning UI can be enabled with the
// |kHttpFormWarningFeature| feature.
bool IsHttpWarningInFormEnabled();

// Returns true for a valid |url| with a cryptographic scheme, e.g., HTTPS, WSS.
bool IsSchemeCryptographic(const GURL& url);

// Returns true for a valid |url| with localhost or file:// scheme origin.
bool IsOriginLocalhostOrFile(const GURL& url);

// Returns true if the page has a valid SSL certificate. Only EV_SECURE,
// SECURE, and SECURE_WITH_POLICY_INSTALLED_CERT are considered valid.
bool IsSslCertificateValid(security_state::SecurityLevel security_level);

}  // namespace security_state

#endif  // COMPONENTS_SECURITY_STATE_CORE_SECURITY_STATE_H_
