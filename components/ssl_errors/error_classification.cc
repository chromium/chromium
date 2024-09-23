// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ssl_errors/error_classification.h"

#include <limits.h>
#include <stddef.h>

#include <vector>

#include "base/build_time.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/levenshtein_distance.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/network_time/network_time_tracker.h"
#include "components/ssl_errors/error_info.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/network_change_notifier.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/cert/x509_certificate.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#endif

using base::Time;
using base::TimeTicks;

namespace ssl_errors {
namespace {

void RecordSSLInterstitialCause(bool overridable, SSLInterstitialCause event) {
  if (overridable) {
    UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.cause.overridable", event,
                              SSL_INTERSTITIAL_CAUSE_MAX);
  } else {
    UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.cause.nonoverridable", event,
                              SSL_INTERSTITIAL_CAUSE_MAX);
  }
}

std::vector<HostnameTokens> GetTokenizedDNSNames(
    const std::vector<std::string>& dns_names) {
  std::vector<HostnameTokens> dns_name_tokens;
  for (const auto& dns_name : dns_names) {
    HostnameTokens dns_name_token_single;
    if (dns_name.empty() || dns_name.find('\0') != std::string::npos ||
        !(HostNameHasKnownTLD(dns_name))) {
      dns_name_token_single.push_back(std::string());
    } else {
      dns_name_token_single = Tokenize(dns_name);
    }
    dns_name_tokens.push_back(dns_name_token_single);
  }
  return dns_name_tokens;
}

// If |potential_subdomain| is a subdomain of |parent|, return the number of
// different tokens. Otherwise returns -1.
int FindSubdomainDifference(const HostnameTokens& potential_subdomain,
                            const HostnameTokens& parent) {
  // A check to ensure that the number of tokens in the tokenized_parent is
  // less than the tokenized_potential_subdomain.
  if (parent.size() >= potential_subdomain.size())
    return -1;

  // Don't be ridiculous. Also, don't overflow.
  if (potential_subdomain.size() >= INT_MAX || parent.size() >= INT_MAX)
    return -1;

  int diff_size = static_cast<int>(potential_subdomain.size() - parent.size());
  for (size_t i = 0; i < parent.size(); i++) {
    if (parent[i] != potential_subdomain[i + diff_size])
      return -1;
  }
  return diff_size;
}

// We accept the inverse case for www for historical reasons.
bool IsWWWSubDomainMatch(const GURL& request_url,
                         const net::X509Certificate& cert) {
  std::string www_host;
  std::vector<std::string> dns_names;
  cert.GetSubjectAltName(&dns_names, nullptr);
  return GetWWWSubDomainMatch(request_url, dns_names, &www_host);
}

// The time to use when doing build time operations in browser tests.
base::LazyInstance<base::Time>::DestructorAtExit g_testing_build_time =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

static ssl_errors::ErrorInfo::ErrorType RecordErrorType(int cert_error) {
  ssl_errors::ErrorInfo::ErrorType error_type =
      ssl_errors::ErrorInfo::NetErrorToErrorType(cert_error);
  UMA_HISTOGRAM_ENUMERATION("interstitial.ssl_error_type", error_type,
                            ssl_errors::ErrorInfo::END_OF_ENUM);
  UMA_HISTOGRAM_ENUMERATION("interstitial.ssl.connection_type",
                            net::NetworkChangeNotifier::GetConnectionType(),
                            net::NetworkChangeNotifier::CONNECTION_LAST);
  return error_type;
}

void RecordUMAStatistics(bool overridable,
                         const base::Time& current_time,
                         const GURL& request_url,
                         int cert_error,
                         const net::X509Certificate& cert) {
  ssl_errors::ErrorInfo::ErrorType error_type = RecordErrorType(cert_error);

  switch (error_type) {
    case ssl_errors::ErrorInfo::CERT_DATE_INVALID: {
      // Note: not reached when displaying the bad clock interstitial.
      // See |RecordUMAStatisticsForClockInterstitial| below.
      if (cert.HasExpired() &&
          (current_time - cert.valid_expiry()).InDays() < 28) {
        RecordSSLInterstitialCause(overridable, EXPIRED_RECENTLY);
      }
      break;
    }
    case ssl_errors::ErrorInfo::CERT_COMMON_NAME_INVALID: {
      std::string host_name = request_url.host();
      std::vector<std::string> dns_names;
      cert.GetSubjectAltName(&dns_names, nullptr);
      std::vector<HostnameTokens> dns_name_tokens =
          GetTokenizedDNSNames(dns_names);

      if (dns_names.empty())
        RecordSSLInterstitialCause(overridable, NO_SUBJECT_ALT_NAME);

      if (HostNameHasKnownTLD(host_name)) {
        HostnameTokens host_name_tokens = Tokenize(host_name);
        if (IsWWWSubDomainMatch(request_url, cert))
          RecordSSLInterstitialCause(overridable, WWW_SUBDOMAIN_MATCH2);
        if (IsSubDomainOutsideWildcard(request_url, cert))
          RecordSSLInterstitialCause(overridable, SUBDOMAIN_OUTSIDE_WILDCARD2);
        if (NameUnderAnyNames(host_name_tokens, dns_name_tokens))
          RecordSSLInterstitialCause(overridable, SUBDOMAIN_MATCH2);
        if (AnyNamesUnderName(dns_name_tokens, host_name_tokens))
          RecordSSLInterstitialCause(overridable, SUBDOMAIN_INVERSE_MATCH2);
        if (IsCertLikelyFromMultiTenantHosting(request_url, cert))
          RecordSSLInterstitialCause(overridable, LIKELY_MULTI_TENANT_HOSTING2);
        if (IsCertLikelyFromSameDomain(request_url, cert))
          RecordSSLInterstitialCause(overridable, LIKELY_SAME_DOMAIN2);
      } else {
        RecordSSLInterstitialCause(overridable, HOST_NAME_NOT_KNOWN_TLD);
      }
      break;
    }
    case ssl_errors::ErrorInfo::CERT_AUTHORITY_INVALID: {
      if (net::IsLocalhost(request_url))
        RecordSSLInterstitialCause(overridable, LOCALHOST);
      const std::string hostname = request_url.HostNoBrackets();
      if (IsHostnameNonUniqueOrDotless(hostname))
        RecordSSLInterstitialCause(overridable, PRIVATE_URL);
      if (net::X509Certificate::IsSelfSigned(cert.cert_buffer()))
        RecordSSLInterstitialCause(overridable, SELF_SIGNED);
      break;
    }
    default:
      break;
  }
}

void RecordUMAStatisticsForClockInterstitial(bool overridable,
                                             ssl_errors::ClockState clock_state,
                                             int cert_error) {
  ssl_errors::ErrorInfo::ErrorType error_type = RecordErrorType(cert_error);
  DCHECK(error_type == ssl_errors::ErrorInfo::CERT_DATE_INVALID);

  if (clock_state == ssl_errors::CLOCK_STATE_FUTURE) {
    RecordSSLInterstitialCause(overridable, CLOCK_FUTURE);
  } else if (clock_state == ssl_errors::CLOCK_STATE_PAST) {
    RecordSSLInterstitialCause(overridable, CLOCK_PAST);
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

ClockState GetClockState(
    const base::Time& now_system,
    const network_time::NetworkTimeTracker* network_time_tracker) {
  base::Time now_network;
  base::TimeDelta uncertainty;
  const base::TimeDelta kNetworkTimeFudge = base::Minutes(5);
  NetworkClockState network_state = NETWORK_CLOCK_STATE_MAX;
  network_time::NetworkTimeTracker::NetworkTimeResult network_time_result =
      network_time_tracker->GetNetworkTime(&now_network, &uncertainty);
  switch (network_time_result) {
    case network_time::NetworkTimeTracker::NETWORK_TIME_AVAILABLE:
      if (now_system < now_network - uncertainty - kNetworkTimeFudge) {
        network_state = NETWORK_CLOCK_STATE_CLOCK_IN_PAST;
      } else if (now_system > now_network + uncertainty + kNetworkTimeFudge) {
        network_state = NETWORK_CLOCK_STATE_CLOCK_IN_FUTURE;
      } else {
        network_state = NETWORK_CLOCK_STATE_OK;
      }
      break;
    case network_time::NetworkTimeTracker::NETWORK_TIME_SYNC_LOST:
      network_state = NETWORK_CLOCK_STATE_UNKNOWN_SYNC_LOST;
      break;
    case network_time::NetworkTimeTracker::NETWORK_TIME_NO_SYNC_ATTEMPT:
      network_state = NETWORK_CLOCK_STATE_UNKNOWN_NO_SYNC_ATTEMPT;
      break;
    case network_time::NetworkTimeTracker::NETWORK_TIME_NO_SUCCESSFUL_SYNC:
      network_state = NETWORK_CLOCK_STATE_UNKNOWN_NO_SUCCESSFUL_SYNC;
      break;
    case network_time::NetworkTimeTracker::NETWORK_TIME_FIRST_SYNC_PENDING:
      network_state = NETWORK_CLOCK_STATE_UNKNOWN_FIRST_SYNC_PENDING;
      break;
    case network_time::NetworkTimeTracker::NETWORK_TIME_SUBSEQUENT_SYNC_PENDING:
      network_state = NETWORK_CLOCK_STATE_UNKNOWN_SUBSEQUENT_SYNC_PENDING;
      break;
  }

  ClockState build_time_state = CLOCK_STATE_UNKNOWN;
  base::Time build_time = g_testing_build_time.Get().is_null()
                              ? base::GetBuildTime()
                              : g_testing_build_time.Get();
  if (now_system < build_time - base::Days(2)) {
    build_time_state = CLOCK_STATE_PAST;
  } else if (now_system > build_time + base::Days(365)) {
    build_time_state = CLOCK_STATE_FUTURE;
  }

  switch (network_state) {
    case NETWORK_CLOCK_STATE_UNKNOWN_SYNC_LOST:
    case NETWORK_CLOCK_STATE_UNKNOWN_NO_SYNC_ATTEMPT:
    case NETWORK_CLOCK_STATE_UNKNOWN_NO_SUCCESSFUL_SYNC:
    case NETWORK_CLOCK_STATE_UNKNOWN_FIRST_SYNC_PENDING:
    case NETWORK_CLOCK_STATE_UNKNOWN_SUBSEQUENT_SYNC_PENDING:
      return build_time_state;
    case NETWORK_CLOCK_STATE_OK:
      return CLOCK_STATE_OK;
    case NETWORK_CLOCK_STATE_CLOCK_IN_PAST:
      return CLOCK_STATE_PAST;
    case NETWORK_CLOCK_STATE_CLOCK_IN_FUTURE:
      return CLOCK_STATE_FUTURE;
    case NETWORK_CLOCK_STATE_MAX:
      NOTREACHED_IN_MIGRATION();
      return CLOCK_STATE_UNKNOWN;
  }

  NOTREACHED_IN_MIGRATION();
  return CLOCK_STATE_UNKNOWN;
}

void SetBuildTimeForTesting(const base::Time& testing_time) {
  g_testing_build_time.Get() = testing_time;
}

bool HostNameHasKnownTLD(const std::string& host_name) {
  return net::registry_controlled_domains::HostHasRegistryControlledDomain(
      host_name, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

HostnameTokens Tokenize(const std::string& name) {
  return base::SplitString(name, ".", base::KEEP_WHITESPACE,
                           base::SPLIT_WANT_ALL);
}

bool GetWWWSubDomainMatch(const GURL& request_url,
                          const std::vector<std::string>& dns_names,
                          std::string* www_match_host_name) {
  const std::string& host_name = request_url.host();

  if (HostNameHasKnownTLD(host_name)) {
    // Need to account for all possible domains given in the SSL certificate.
    for (const auto& dns_name : dns_names) {
      if (dns_name.empty() || dns_name.find('\0') != std::string::npos ||
          dns_name.length() == host_name.length() ||
          !HostNameHasKnownTLD(dns_name)) {
        continue;
      } else if (dns_name.length() > host_name.length()) {
        if (url_formatter::StripWWW(dns_name) == host_name) {
          *www_match_host_name = dns_name;
          return true;
        }
      } else {
        if (url_formatter::StripWWW(host_name) == dns_name) {
          *www_match_host_name = dns_name;
          return true;
        }
      }
    }
  }
  return false;
}

bool NameUnderAnyNames(const HostnameTokens& child,
                       const std::vector<HostnameTokens>& potential_parents) {
  // Need to account for all the possible domains given in the SSL certificate.
  for (const auto& potential_parent : potential_parents) {
    if (potential_parent.empty() || potential_parent.size() >= child.size()) {
      continue;
    }
    int domain_diff = FindSubdomainDifference(child, potential_parent);
    if (domain_diff == 1 && child[0] != "www")
      return true;
  }
  return false;
}

bool AnyNamesUnderName(const std::vector<HostnameTokens>& potential_children,
                       const HostnameTokens& parent) {
  // Need to account for all the possible domains given in the SSL certificate.
  for (const auto& potential_child : potential_children) {
    if (potential_child.empty() || potential_child.size() <= parent.size()) {
      continue;
    }
    int domain_diff = FindSubdomainDifference(potential_child, parent);
    if (domain_diff == 1 && potential_child[0] != "www")
      return true;
  }
  return false;
}

bool IsSubDomainOutsideWildcard(const GURL& request_url,
                                const net::X509Certificate& cert) {
  std::string host_name = request_url.host();
  HostnameTokens host_name_tokens = Tokenize(host_name);
  std::vector<std::string> dns_names;
  cert.GetSubjectAltName(&dns_names, nullptr);
  bool result = false;

  // This method requires that the host name be longer than the dns name on
  // the certificate.
  for (const auto& dns_name : dns_names) {
    if (dns_name.length() < 2 || dns_name.length() >= host_name.length() ||
        dns_name.find('\0') != std::string::npos ||
        !HostNameHasKnownTLD(dns_name) || dns_name[0] != '*' ||
        dns_name[1] != '.') {
      continue;
    }

    // Move past the "*.".
    std::string extracted_dns_name = dns_name.substr(2);
    if (FindSubdomainDifference(host_name_tokens,
                                Tokenize(extracted_dns_name)) == 2) {
      return true;
    }
  }
  return result;
}

bool IsCertLikelyFromMultiTenantHosting(const GURL& request_url,
                                        const net::X509Certificate& cert) {
  std::string host_name = request_url.host();
  std::vector<std::string> dns_names;
  std::vector<std::string> dns_names_domain;
  cert.GetSubjectAltName(&dns_names, nullptr);
  size_t dns_names_size = dns_names.size();

  // If there is only 1 DNS name then it is definitely not a shared certificate.
  if (dns_names_size == 0 || dns_names_size == 1)
    return false;

  // Check to see if all the domains in the SAN field in the SSL certificate are
  // the same or not.
  for (size_t i = 0; i < dns_names_size; ++i) {
    dns_names_domain.push_back(
        net::registry_controlled_domains::GetDomainAndRegistry(
            dns_names[i],
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  }
  for (size_t i = 1; i < dns_names_domain.size(); ++i) {
    if (dns_names_domain[i] != dns_names_domain[0])
      return false;
  }

  // If the number of DNS names is more than 5 then assume that it is a shared
  // certificate.
  static const int kDistinctNameThreshold = 5;
  if (dns_names_size > kDistinctNameThreshold)
    return true;

  // Heuristic - The edit distance between all the strings should be at least 5
  // for it to be counted as a shared SSLCertificate. If even one pair of
  // strings edit distance is below 5 then the certificate is no longer
  // considered as a shared certificate. Include the host name in the URL also
  // while comparing.
  dns_names.push_back(host_name);
  static const size_t kMinimumEditDistance = 5;
  for (size_t i = 0; i < dns_names_size; ++i) {
    for (size_t j = i + 1; j < dns_names_size; ++j) {
      if (base::LevenshteinDistance(dns_names[i], dns_names[j],
                                    kMinimumEditDistance - 1) <
          kMinimumEditDistance) {
        return false;
      }
    }
  }
  return true;
}

bool IsCertLikelyFromSameDomain(const GURL& request_url,
                                const net::X509Certificate& cert) {
  std::string host_name = request_url.host();
  std::vector<std::string> dns_names;
  cert.GetSubjectAltName(&dns_names, nullptr);
  if (dns_names.empty())
    return false;

  dns_names.push_back(host_name);
  std::vector<std::string> dns_names_domain;

  for (const std::string& dns_name : dns_names) {
    dns_names_domain.push_back(
        net::registry_controlled_domains::GetDomainAndRegistry(
            dns_name,
            net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES));
  }

  DCHECK(!dns_names_domain.empty());
  const std::string& host_name_domain = dns_names_domain.back();

  // Last element is the original domain. So, excluding it.
  return std::find(dns_names_domain.begin(), dns_names_domain.end() - 1,
                   host_name_domain) != dns_names_domain.end() - 1;
}

bool IsHostnameNonUniqueOrDotless(const std::string& hostname) {
  return net::IsHostnameNonUnique(hostname) ||
         hostname.find('.') == std::string::npos;
}

}  // namespace ssl_errors
