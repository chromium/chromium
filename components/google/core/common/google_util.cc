// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/google/core/common/google_util.h"

#include <stddef.h>

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/no_destructor.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "components/google/core/common/google_switches.h"
#include "components/google/core/common/google_tld_list.h"
#include "components/url_formatter/url_fixer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

// Only use Link Doctor on official builds.  It uses an API key, too, but
// seems best to just disable it, for more responsive error pages and to reduce
// server load.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#define LINKDOCTOR_SERVER_REQUEST_URL "https://www.googleapis.com/rpc"
#else
#define LINKDOCTOR_SERVER_REQUEST_URL ""
#endif

namespace google_util {

// Helpers --------------------------------------------------------------------

namespace {

bool gUseMockLinkDoctorBaseURLForTesting = false;

bool g_ignore_port_numbers = false;

bool IsPathHomePageBase(base::StringPiece path) {
  return (path == "/") || (path == "/webhp");
}

// Removes a single trailing dot if present in |host|.
void StripTrailingDot(base::StringPiece* host) {
  if (host->ends_with("."))
    host->remove_suffix(1);
}

// True if the given canonical |host| is "[www.]<domain_in_lower_case>.<TLD>"
// with a valid TLD. If |subdomain_permission| is ALLOW_SUBDOMAIN, we check
// against host "*.<domain_in_lower_case>.<TLD>" instead. Will return the TLD
// string in |tld|, if specified and the |host| can be parsed.
bool IsValidHostName(base::StringPiece host,
                     base::StringPiece domain_in_lower_case,
                     SubdomainPermission subdomain_permission,
                     base::StringPiece* tld) {
  // Fast path to avoid searching the registry set.
  if (host.find(domain_in_lower_case) == base::StringPiece::npos)
    return false;

  size_t tld_length =
      net::registry_controlled_domains::GetCanonicalHostRegistryLength(
          host, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  if ((tld_length == 0) || (tld_length == std::string::npos))
    return false;

  // Removes the tld and the preceding dot.
  base::StringPiece host_minus_tld =
      host.substr(0, host.length() - tld_length - 1);

  if (tld)
    *tld = host.substr(host.length() - tld_length);

  if (base::LowerCaseEqualsASCII(host_minus_tld, domain_in_lower_case))
    return true;

  if (subdomain_permission == ALLOW_SUBDOMAIN) {
    std::string dot_domain(".");
    domain_in_lower_case.AppendToString(&dot_domain);
    return base::EndsWith(host_minus_tld, dot_domain,
                          base::CompareCase::INSENSITIVE_ASCII);
  }

  std::string www_domain("www.");
  domain_in_lower_case.AppendToString(&www_domain);
  return base::LowerCaseEqualsASCII(host_minus_tld, www_domain);
}

// True if |url| is a valid URL with HTTP or HTTPS scheme. If |port_permission|
// is DISALLOW_NON_STANDARD_PORTS, this also requires |url| to use the standard
// port for its scheme (80 for HTTP, 443 for HTTPS).
bool IsValidURL(const GURL& url, PortPermission port_permission) {
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS() &&
         (url.port().empty() || g_ignore_port_numbers ||
          (port_permission == ALLOW_NON_STANDARD_PORTS));
}

bool IsCanonicalHostGoogleHostname(base::StringPiece canonical_host,
                                   SubdomainPermission subdomain_permission) {
  const GURL& base_url(CommandLineGoogleBaseURL());
  if (base_url.is_valid() && (canonical_host == base_url.host_piece()))
    return true;

  base::StringPiece tld;
  if (!IsValidHostName(canonical_host, "google", subdomain_permission, &tld))
    return false;

  // Remove the trailing dot from tld if present, as for google domain it's the
  // same page.
  StripTrailingDot(&tld);

  static const base::NoDestructor<base::flat_set<base::StringPiece>>
      google_tlds(std::initializer_list<base::StringPiece>({GOOGLE_TLD_LIST}));
  return google_tlds->contains(tld);
}

// True if |url| is a valid URL with a host that is in the static list of
// Google subdomains for google search, and an HTTP or HTTPS scheme. Requires
// |url| to use the standard port for its scheme (80 for HTTP, 443 for HTTPS).
bool IsGoogleSearchSubdomainUrl(const GURL& url) {
  if (!IsValidURL(url, PortPermission::DISALLOW_NON_STANDARD_PORTS))
    return false;

  base::StringPiece host(url.host_piece());
  StripTrailingDot(&host);

  static const base::NoDestructor<base::flat_set<base::StringPiece>>
      google_subdomains(std::initializer_list<base::StringPiece>(
          {"ipv4.google.com", "ipv6.google.com"}));

  return google_subdomains->contains(host);
}

}  // namespace

// Global functions -----------------------------------------------------------

const char kGoogleHomepageURL[] = "https://www.google.com/";

bool HasGoogleSearchQueryParam(base::StringPiece str) {
  url::Component query(0, static_cast<int>(str.length())), key, value;
  while (url::ExtractQueryKeyValue(str.data(), &query, &key, &value)) {
    base::StringPiece key_str = str.substr(key.begin, key.len);
    if (key_str == "q" || key_str == "as_q")
      return true;
  }
  return false;
}

GURL LinkDoctorBaseURL() {
  if (gUseMockLinkDoctorBaseURLForTesting)
    return GURL("http://mock.linkdoctor.url/for?testing");
  return GURL(LINKDOCTOR_SERVER_REQUEST_URL);
}

void SetMockLinkDoctorBaseURLForTesting() {
  gUseMockLinkDoctorBaseURLForTesting = true;
}

std::string GetGoogleLocale(const std::string& application_locale) {
  // Google does not recognize "nb" for Norwegian Bokmal; it uses "no".
  return (application_locale == "nb") ? "no" : application_locale;
}

GURL AppendGoogleLocaleParam(const GURL& url,
                             const std::string& application_locale) {
  return net::AppendQueryParameter(url, "hl",
                                   GetGoogleLocale(application_locale));
}

std::string GetGoogleCountryCode(const GURL& google_homepage_url) {
  base::StringPiece google_hostname = google_homepage_url.host_piece();
  // TODO(igorcov): This needs a fix for case when the host has a trailing dot,
  // like "google.com./". https://crbug.com/720295.
  const size_t last_dot = google_hostname.find_last_of('.');
  if (last_dot == std::string::npos)
    return std::string();
  base::StringPiece country_code = google_hostname.substr(last_dot + 1);
  // Assume the com TLD implies the US.
  if (country_code == "com")
    return "us";
  // Google uses the Unicode Common Locale Data Repository (CLDR), and the CLDR
  // code for the UK is "gb".
  if (country_code == "uk")
    return "gb";
  // Catalonia does not have a CLDR country code, since it's a region in Spain,
  // so use Spain instead.
  if (country_code == "cat")
    return "es";
  return country_code.as_string();
}

GURL GetGoogleSearchURL(const GURL& google_homepage_url) {
  // To transform the homepage URL into the corresponding search URL, add the
  // "search" and the "q=" query string.
  GURL::Replacements replacements;
  replacements.SetPathStr("search");
  replacements.SetQueryStr("q=");
  return google_homepage_url.ReplaceComponents(replacements);
}

const GURL& CommandLineGoogleBaseURL() {
  // Unit tests may add command-line flags after the first call to this
  // function, so we don't simply initialize a static |base_url| directly and
  // then unconditionally return it.
  static base::NoDestructor<std::string> switch_value;
  static base::NoDestructor<GURL> base_url;
  std::string current_switch_value(
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kGoogleBaseURL));
  if (current_switch_value != *switch_value) {
    *switch_value = current_switch_value;
    *base_url = url_formatter::FixupURL(*switch_value, std::string());
    if (!base_url->is_valid() || base_url->has_query() || base_url->has_ref())
      *base_url = GURL();
  }
  return *base_url;
}

bool StartsWithCommandLineGoogleBaseURL(const GURL& url) {
  const GURL& base_url(CommandLineGoogleBaseURL());
  return base_url.is_valid() &&
         base::StartsWith(url.possibly_invalid_spec(), base_url.spec(),
                          base::CompareCase::SENSITIVE);
}

bool IsGoogleHostname(base::StringPiece host,
                      SubdomainPermission subdomain_permission) {
  url::CanonHostInfo host_info;
  return IsCanonicalHostGoogleHostname(net::CanonicalizeHost(host, &host_info),
                                       subdomain_permission);
}

bool IsGoogleDomainUrl(const GURL& url,
                       SubdomainPermission subdomain_permission,
                       PortPermission port_permission) {
  return IsValidURL(url, port_permission) &&
         IsCanonicalHostGoogleHostname(url.host_piece(), subdomain_permission);
}

bool IsGoogleHomePageUrl(const GURL& url) {
  // First check to see if this has a Google domain.
  if (!IsGoogleDomainUrl(url, DISALLOW_SUBDOMAIN,
                         DISALLOW_NON_STANDARD_PORTS) &&
      !IsGoogleSearchSubdomainUrl(url)) {
    return false;
  }

  // Make sure the path is a known home page path.
  base::StringPiece path(url.path_piece());
  return IsPathHomePageBase(path) ||
         base::StartsWith(path, "/ig", base::CompareCase::INSENSITIVE_ASCII);
}

bool IsGoogleSearchUrl(const GURL& url) {
  // First check to see if this has a Google domain.
  if (!IsGoogleDomainUrl(url, DISALLOW_SUBDOMAIN,
                         DISALLOW_NON_STANDARD_PORTS) &&
      !IsGoogleSearchSubdomainUrl(url)) {
    return false;
  }

  // Make sure the path is a known search path.
  base::StringPiece path(url.path_piece());
  bool is_home_page_base = IsPathHomePageBase(path);
  if (!is_home_page_base && (path != "/search"))
    return false;

  // Check for query parameter in URL parameter and hash fragment, depending on
  // the path type.
  return HasGoogleSearchQueryParam(url.ref_piece()) ||
         (!is_home_page_base && HasGoogleSearchQueryParam(url.query_piece()));
}

bool IsYoutubeDomainUrl(const GURL& url,
                        SubdomainPermission subdomain_permission,
                        PortPermission port_permission) {
  return IsValidURL(url, port_permission) &&
         IsValidHostName(url.host_piece(), "youtube", subdomain_permission,
                         nullptr);
}

bool IsGoogleAssociatedDomainUrl(const GURL& url) {
  if (IsGoogleDomainUrl(url, ALLOW_SUBDOMAIN, ALLOW_NON_STANDARD_PORTS))
    return true;

  if (IsYoutubeDomainUrl(url, ALLOW_SUBDOMAIN, ALLOW_NON_STANDARD_PORTS))
    return true;

  // Some domains don't have international TLD extensions, so testing for them
  // is very straightforward.
  static const char* kSuffixesToSetHeadersFor[] = {
      ".android.com",
      ".doubleclick.com",
      ".doubleclick.net",
      ".ggpht.com",
      ".googleadservices.com",
      ".googleapis.com",
      ".googlesyndication.com",
      ".googleusercontent.com",
      ".googlevideo.com",
      ".gstatic.com",
      ".litepages.googlezip.net",
      ".ytimg.com",
  };
  const std::string host = url.host();
  for (size_t i = 0; i < base::size(kSuffixesToSetHeadersFor); ++i) {
    if (base::EndsWith(host, kSuffixesToSetHeadersFor[i],
                       base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }

  // Exact hostnames in lowercase to set headers for.
  static const char* kHostsToSetHeadersFor[] = {
      "googleweblight.com",
  };
  for (size_t i = 0; i < base::size(kHostsToSetHeadersFor); ++i) {
    if (base::LowerCaseEqualsASCII(host, kHostsToSetHeadersFor[i]))
      return true;
  }

  return false;
}

const std::vector<std::string>& GetGoogleRegistrableDomains() {
  static base::NoDestructor<std::vector<std::string>>
      kGoogleRegisterableDomains([]() {
        std::vector<std::string> domains;

        std::vector<std::string> tlds{GOOGLE_TLD_LIST};
        for (const std::string& tld : tlds) {
          std::string domain = "google." + tld;

          // The Google TLD list might contain domains that are not considered
          // to be registrable domains by net::registry_controlled_domains.
          if (GetDomainAndRegistry(domain,
                                   net::registry_controlled_domains::
                                       INCLUDE_PRIVATE_REGISTRIES) != domain) {
            continue;
          }

          domains.push_back(domain);
        }

        return domains;
      }());

  return *kGoogleRegisterableDomains;
}

void IgnorePortNumbersForGoogleURLChecksForTesting() {
  g_ignore_port_numbers = true;
}

}  // namespace google_util
