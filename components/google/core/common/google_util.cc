// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/google/core/common/google_util.h"

#include <stddef.h>

#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/containers/fixed_flat_set.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/google/core/common/google_switches.h"
#include "components/google/core/common/google_tld_list.h"
#include "components/url_formatter/url_fixer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace google_util {

// Helpers --------------------------------------------------------------------

namespace {

bool IsPathHomePageBase(std::string_view path) {
  return (path == "/") || (path == "/webhp");
}

// Removes a single trailing dot if present in |host|.
void StripTrailingDot(std::string_view* host) {
  if (base::EndsWith(*host, "."))
    host->remove_suffix(1);
}

// True if the given canonical |host| is "[www.]<domain_in_lower_case>.<TLD>"
// with a valid TLD that appears in |allowed_tlds|. If |subdomain_permission| is
// ALLOW_SUBDOMAIN, we check against host "*.<domain_in_lower_case>.<TLD>"
// instead.
template <typename Container>
bool IsValidHostName(std::string_view host,
                     std::string_view domain_in_lower_case,
                     SubdomainPermission subdomain_permission,
                     const Container& allowed_tlds) {
  // Fast path to avoid searching the registry set.
  if (host.find(domain_in_lower_case) == std::string_view::npos) {
    return false;
  }

  size_t tld_length =
      net::registry_controlled_domains::GetCanonicalHostRegistryLength(
          host, net::registry_controlled_domains::EXCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
  if ((tld_length == 0) || (tld_length == std::string::npos))
    return false;

  // Removes the tld and the preceding dot.
  std::string_view host_minus_tld =
      host.substr(0, host.length() - tld_length - 1);

  std::string_view tld = host.substr(host.length() - tld_length);
  // Remove the trailing dot from tld if present, as for Google domains it's the
  // same page.
  StripTrailingDot(&tld);
  if (!allowed_tlds.contains(tld))
    return false;

  if (base::EqualsCaseInsensitiveASCII(host_minus_tld, domain_in_lower_case))
    return true;

  if (subdomain_permission == ALLOW_SUBDOMAIN) {
    std::string dot_domain = base::StrCat({".", domain_in_lower_case});
    return base::EndsWith(host_minus_tld, dot_domain,
                          base::CompareCase::INSENSITIVE_ASCII);
  }

  std::string www_domain = base::StrCat({"www.", domain_in_lower_case});
  return base::EqualsCaseInsensitiveASCII(host_minus_tld, www_domain);
}

// True if |url| is a valid URL with HTTP or HTTPS scheme. If |port_permission|
// is DISALLOW_NON_STANDARD_PORTS, this also requires |url| to use the standard
// port for its scheme (80 for HTTP, 443 for HTTPS).
bool IsValidURL(const GURL& url, PortPermission port_permission) {
  static bool g_ignore_port_numbers =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kIgnoreGooglePortNumbers);
  return url.is_valid() && url.SchemeIsHTTPOrHTTPS() &&
         (url.port().empty() || g_ignore_port_numbers ||
          (port_permission == ALLOW_NON_STANDARD_PORTS));
}

bool IsCanonicalHostGoogleHostname(std::string_view canonical_host,
                                   SubdomainPermission subdomain_permission) {
  const GURL& base_url(CommandLineGoogleBaseURL());
  if (base_url.is_valid() && (canonical_host == base_url.host_piece()))
    return true;

  static constexpr auto google_tlds =
      base::MakeFixedFlatSet<std::string_view>({GOOGLE_TLD_LIST});
  return IsValidHostName(canonical_host, "google", subdomain_permission,
                         google_tlds);
}

bool IsCanonicalHostYoutubeHostname(std::string_view canonical_host,
                                    SubdomainPermission subdomain_permission) {
  static constexpr auto youtube_tlds =
      base::MakeFixedFlatSet<std::string_view>({YOUTUBE_TLD_LIST});

  return IsValidHostName(canonical_host, "youtube", subdomain_permission,
                         youtube_tlds);
}

// True if |url| is a valid URL with a host that is in the static list of
// Google subdomains for google search, and an HTTP or HTTPS scheme. Requires
// |url| to use the standard port for its scheme (80 for HTTP, 443 for HTTPS).
bool IsGoogleSearchSubdomainUrl(const GURL& url) {
  if (!IsValidURL(url, PortPermission::DISALLOW_NON_STANDARD_PORTS))
    return false;

  std::string_view host(url.host_piece());
  StripTrailingDot(&host);

  static constexpr auto google_subdomains =
      base::MakeFixedFlatSet<std::string_view>(
          {"ipv4.google.com", "ipv6.google.com"});

  return google_subdomains.contains(host);
}

}  // namespace

// Global functions -----------------------------------------------------------

const char kGoogleHomepageURL[] = "https://www.google.com/";

bool HasGoogleSearchQueryParam(std::string_view str) {
  url::Component query(0, static_cast<int>(str.length())), key, value;
  while (url::ExtractQueryKeyValue(str, &query, &key, &value)) {
    std::string_view key_str = str.substr(key.begin, key.len);
    if (key_str == "q" || key_str == "as_q" || key_str == "imgurl")
      return true;
  }
  return false;
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
  std::string_view google_hostname = google_homepage_url.host_piece();
  // TODO(igorcov): This needs a fix for case when the host has a trailing dot,
  // like "google.com./". https://crbug.com/720295.
  const size_t last_dot = google_hostname.find_last_of('.');
  if (last_dot == std::string::npos)
    return std::string();
  std::string_view country_code = google_hostname.substr(last_dot + 1);
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
  return std::string(country_code);
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

bool IsGoogleDomainUrl(const GURL& url,
                       SubdomainPermission subdomain_permission,
                       PortPermission port_permission) {
  return IsValidURL(url, port_permission) &&
         IsCanonicalHostGoogleHostname(url.host_piece(), subdomain_permission);
}

bool IsGoogleHostname(std::string_view host,
                      SubdomainPermission subdomain_permission) {
  url::CanonHostInfo host_info;
  return IsCanonicalHostGoogleHostname(net::CanonicalizeHost(host, &host_info),
                                       subdomain_permission);
}

bool IsGoogleHomePageUrl(const GURL& url) {
  // First check to see if this has a Google domain.
  if (!IsGoogleDomainUrl(url, DISALLOW_SUBDOMAIN,
                         DISALLOW_NON_STANDARD_PORTS) &&
      !IsGoogleSearchSubdomainUrl(url)) {
    return false;
  }

  // Make sure the path is a known home page path.
  std::string_view path(url.path_piece());
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
  std::string_view path(url.path_piece());
  bool is_home_page_base = IsPathHomePageBase(path);
  if (!is_home_page_base && path != "/search" && path != "/imgres")
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
         IsCanonicalHostYoutubeHostname(url.host_piece(), subdomain_permission);
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
      ".youtubekids.com",
      ".ytimg.com",
  };
  const std::string host = url.host();
  for (size_t i = 0; i < std::size(kSuffixesToSetHeadersFor); ++i) {
    if (base::EndsWith(host, kSuffixesToSetHeadersFor[i],
                       base::CompareCase::INSENSITIVE_ASCII)) {
      return true;
    }
  }

  // Exact hostnames in lowercase to set headers for.
  static const char* kHostsToSetHeadersFor[] = {
      "googleweblight.com",
  };
  for (size_t i = 0; i < std::size(kHostsToSetHeadersFor); ++i) {
    if (base::EqualsCaseInsensitiveASCII(host, kHostsToSetHeadersFor[i]))
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

GURL AppendToAsyncQueryParam(const GURL& url,
                             const std::string& key,
                             const std::string& value) {
  const std::string param_name = "async";
  const std::string key_value = key + ":" + value;
  bool replaced = false;
  const std::string_view input = url.query_piece();
  url::Component cursor(0, input.size());
  std::string output;
  url::Component key_range, value_range;
  while (url::ExtractQueryKeyValue(input, &cursor, &key_range, &value_range)) {
    const std::string_view input_key =
        input.substr(key_range.begin, key_range.len);
    std::string key_value_pair(
        input.substr(key_range.begin, value_range.end() - key_range.begin));
    if (!replaced && input_key == param_name) {
      // Check |replaced| as only the first match should be replaced.
      replaced = true;
      key_value_pair += "," + key_value;
    }
    if (!output.empty()) {
      output += "&";
    }

    output += key_value_pair;
  }
  if (!replaced) {
    if (!output.empty()) {
      output += "&";
    }

    output += (param_name + "=" + key_value);
  }
  GURL::Replacements replacements;
  replacements.SetQueryStr(output);
  return url.ReplaceComponents(replacements);
}

GoogleSearchMode GoogleSearchModeFromUrl(const GURL& url) {
  static_assert(GoogleSearchMode::kMaxValue == GoogleSearchMode::kFlights,
                "This function should be updated if new values are added to "
                "GoogleSearchMode");

  std::string_view query_str = url.query_piece();
  url::Component query(0, static_cast<int>(url.query_piece().length()));
  url::Component key, value;
  GoogleSearchMode mode = GoogleSearchMode::kUnspecified;
  while (url::ExtractQueryKeyValue(query_str, &query, &key, &value)) {
    std::string_view key_str = query_str.substr(key.begin, key.len);
    if (key_str != "tbm") {
      continue;
    }
    if (mode != GoogleSearchMode::kUnspecified) {
      // There is more than one tbm parameter, which is not expected. Return
      // kUnknown to signify the result can't be trusted.
      return GoogleSearchMode::kUnknown;
    }
    std::string_view value_str = query_str.substr(value.begin, value.len);
    if (value_str == "isch") {
      mode = GoogleSearchMode::kImages;
    } else if (value_str == "web") {
      mode = GoogleSearchMode::kWeb;
    } else if (value_str == "nws") {
      mode = GoogleSearchMode::kNews;
    } else if (value_str == "shop") {
      mode = GoogleSearchMode::kShopping;
    } else if (value_str == "vid") {
      mode = GoogleSearchMode::kVideos;
    } else if (value_str == "bks") {
      mode = GoogleSearchMode::kBooks;
    } else if (value_str == "flm") {
      mode = GoogleSearchMode::kFlights;
    } else if (value_str == "lcl") {
      mode = GoogleSearchMode::kLocal;
    } else {
      mode = GoogleSearchMode::kUnknown;
    }
  }
  if (mode == GoogleSearchMode::kUnspecified) {
    // No tbm query parameter means this is the Web mode.
    mode = GoogleSearchMode::kWeb;
  }
  return mode;
}

}  // namespace google_util
