// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SSL_ERRORS_ERROR_CLASSIFICATION_H_
#define COMPONENTS_SSL_ERRORS_ERROR_CLASSIFICATION_H_

#include <string>
#include <vector>

namespace base {
class Time;
}

class GURL;

namespace net {
class X509Certificate;
}

namespace network_time {
class NetworkTimeTracker;
}

namespace ssl_errors {

typedef std::vector<std::string> HostnameTokens;

// Methods for identifying specific error causes. ------------------------------

// These values are written to logs. New enum values can be added, but existing
// enums must never be renumbered or deleted and reused.
enum SSLInterstitialCause {
  CLOCK_PAST = 0,
  CLOCK_FUTURE = 1,
  WWW_SUBDOMAIN_MATCH = 2,         // Deprecated in M59.
  SUBDOMAIN_MATCH = 3,             // Deprecated in M59.
  SUBDOMAIN_INVERSE_MATCH = 4,     // Deprecated in M59.
  SUBDOMAIN_OUTSIDE_WILDCARD = 5,  // Deprecated in M59.
  HOST_NAME_NOT_KNOWN_TLD = 6,
  LIKELY_MULTI_TENANT_HOSTING = 7,  // Deprecated in M59.
  LOCALHOST = 8,
  PRIVATE_URL = 9,
  AUTHORITY_ERROR_CAPTIVE_PORTAL = 10,  // Deprecated in M47.
  SELF_SIGNED = 11,
  EXPIRED_RECENTLY = 12,
  LIKELY_SAME_DOMAIN = 13,  // Deprecated in M59.
  NO_SUBJECT_ALT_NAME = 14,
  WWW_SUBDOMAIN_MATCH2 = 15,
  SUBDOMAIN_MATCH2 = 16,
  SUBDOMAIN_INVERSE_MATCH2 = 17,
  SUBDOMAIN_OUTSIDE_WILDCARD2 = 18,
  LIKELY_MULTI_TENANT_HOSTING2 = 19,
  LIKELY_SAME_DOMAIN2 = 20,
  SSL_INTERSTITIAL_CAUSE_MAX
};

// What is known about the accuracy of system clock.  Do not change or
// reorder; these values are used in an UMA histogram.
enum ClockState {
  // Not known whether system clock is close enough.
  CLOCK_STATE_UNKNOWN,

  // System clock is "close enough", per network time.
  CLOCK_STATE_OK,

  // System clock is behind.
  CLOCK_STATE_PAST,

  // System clock is ahead.
  CLOCK_STATE_FUTURE,

  CLOCK_STATE_MAX,
};

// Describes the result of getting network time and if it was
// unavailable, why it was unavailable. This enum is being histogrammed
// so do not reorder or remove values.
//
// Exposed for testing.
enum NetworkClockState {
  // Value 0 was NETWORK_CLOCK_STATE_UNKNOWN_NO_SYNC, which is obsolete
  // in favor of the finer-grained values below.

  // The clock state relative to network time is unknown because the
  // user's clock has fallen out of sync with the latest information
  // from the network (due to e.g. suspend/resume).
  NETWORK_CLOCK_STATE_UNKNOWN_SYNC_LOST = 1,
  // The clock is "close enough" to the network time.
  NETWORK_CLOCK_STATE_OK,
  // The clock is in the past relative to network time.
  NETWORK_CLOCK_STATE_CLOCK_IN_PAST,
  // The clock is in the future relative to network time.
  NETWORK_CLOCK_STATE_CLOCK_IN_FUTURE,
  // The clock state relative to network time is unknown because no sync
  // attempt has been made yet.
  NETWORK_CLOCK_STATE_UNKNOWN_NO_SYNC_ATTEMPT,
  // The clock state relative to network time is unknown because one or
  // more sync attempts has failed.
  NETWORK_CLOCK_STATE_UNKNOWN_NO_SUCCESSFUL_SYNC,
  // The clock state relative to network time is unknown because the
  // first sync attempt is still pending.
  NETWORK_CLOCK_STATE_UNKNOWN_FIRST_SYNC_PENDING,
  // The clock state relative to network time is unknown because one or
  // more time query attempts have failed, and a subsequent sync attempt
  // is still pending.
  NETWORK_CLOCK_STATE_UNKNOWN_SUBSEQUENT_SYNC_PENDING,
  NETWORK_CLOCK_STATE_MAX
};

// Compares |now_system| to the build time and to the current network time, and
// returns an inference about the state of the system clock.  A result from
// network time, if available, will always be preferred to a result from the
// build time.  Calling this function records UMA statistics: it's assumed that
// it's called in the course of handling an SSL error.
ClockState GetClockState(
    const base::Time& now_system,
    const network_time::NetworkTimeTracker* network_time_tracker);

// Returns true if |hostname| is too broad for the scope of a wildcard
// certificate. E.g.:
//     a.b.example.com ~ *.example.com --> true
//     b.example.com ~ *.example.com --> false
bool IsSubDomainOutsideWildcard(const GURL& request_url,
                                const net::X509Certificate& cert);

// Returns true if the certificate is a shared certificate. Note - This
// function should be used with caution (only for UMA histogram) as an
// attacker could easily get a certificate with more than 5 names in the SAN
// fields.
bool IsCertLikelyFromMultiTenantHosting(const GURL& request_url,
                                        const net::X509Certificate& cert);

// Returns true if the hostname in |request_url_| has the same domain
// (effective TLD + 1 label) as at least one of the subject
// alternative names in |cert_|.
bool IsCertLikelyFromSameDomain(const GURL& request_url,
                                const net::X509Certificate& cert);

// Returns true if the site's hostname differs from one of the DNS names in
// |dns_names| only by the presence or absence of the single-label prefix "www".
// The matching name from the certificate is returned in |www_match_host_name|.
bool GetWWWSubDomainMatch(const GURL& request_url,
                          const std::vector<std::string>& dns_names,
                          std::string* www_match_host_name);

// Method for recording results. -----------------------------------------------

void RecordUMAStatistics(bool overridable,
                         const base::Time& current_time,
                         const GURL& request_url,
                         int cert_error,
                         const net::X509Certificate& cert);

// Specialization of |RecordUMAStatistics| to be used when the bad clock
// interstitial is shown.  |cert_error| is required only for sanity-checking: it
// must always be |ssl_errors::ErrorInfo::CERT_DATE_INVALID|.
void RecordUMAStatisticsForClockInterstitial(bool overridable,
                                             ssl_errors::ClockState clock_state,
                                             int cert_error);

// Helper methods for classification. ------------------------------------------

// Tokenize DNS names and hostnames.
HostnameTokens Tokenize(const std::string& name);

// Sets a clock for browser tests that check the build time. Used by
// GetClockState().
void SetBuildTimeForTesting(const base::Time& testing_time);

// Returns true if the hostname has a known Top Level Domain.
bool HostNameHasKnownTLD(const std::string& host_name);

// Returns true if any one of the following conditions hold:
// 1.|hostname| is an IP Address in an IANA-reserved range.
// 2.|hostname| is a not-yet-assigned by ICANN gTLD.
// 3.|hostname| is a dotless domain.
bool IsHostnameNonUniqueOrDotless(const std::string& hostname);

// Returns true if |child| is a subdomain of any of the |potential_parents|.
bool NameUnderAnyNames(const HostnameTokens& child,
                       const std::vector<HostnameTokens>& potential_parents);

// Returns true if any of the |potential_children| is a subdomain of the
// |parent|. The inverse case should be treated carefully as this is most
// likely a MITM attack. We don't want foo.appspot.com to be able to MITM for
// appspot.com.
bool AnyNamesUnderName(const std::vector<HostnameTokens>& potential_children,
                       const HostnameTokens& parent);

}  // namespace ssl_errors

#endif  // COMPONENTS_SSL_ERRORS_ERROR_CLASSIFICATION_H_
