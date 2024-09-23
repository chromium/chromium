// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Some Google related utility functions.

#ifndef COMPONENTS_GOOGLE_CORE_COMMON_GOOGLE_UTIL_H_
#define COMPONENTS_GOOGLE_CORE_COMMON_GOOGLE_UTIL_H_

#include <string>
#include <string_view>
#include <vector>

class GURL;

// This namespace provides various helpers around handling Google-related URLs.
namespace google_util {

// The Google Search mode of a page. This corresponds to the tab (e.g. web
// result, image results, video results, etc.) the user is on. Used in UKM
// logging so don't remove or reorder values. Update |GoogleSearchMode| in
// //tools/metrics/histograms/enums.xml and |GoogleSearchModeFromUrl| whenever
// this is changed.
enum class GoogleSearchMode {
  // Sentinel for uninitialized values in UKM.
  kUnspecified = 0,
  // We do not know the mode used. Either the tbm query parameter did not match
  // any of the known Search modes or there was more than one tbm query
  // parameter, which is not expected.
  kUnknown = 1,
  kWeb = 2,
  kImages = 3,
  kNews = 4,
  kShopping = 5,
  kVideos = 6,
  kBooks = 7,
  kLocal = 8,
  kFlights = 9,
  kMaxValue = kFlights,
};

extern const char kGoogleHomepageURL[];

// True iff |str| contains a "q=" or "as_q=" query parameter with a non-empty
// value. |str| should be a query or a hash fragment, without the ? or # (as
// returned by GURL::query() or GURL::ref().
bool HasGoogleSearchQueryParam(std::string_view str);

// Returns the Google locale corresponding to |application_locale|.  This is
// the same string as AppendGoogleLocaleParam adds to the URL, only without the
// leading "hl".
std::string GetGoogleLocale(const std::string& application_locale);

// Adds the Google locale string to the URL (e.g., hl=en-US).  This does not
// check to see if the param already exists.
GURL AppendGoogleLocaleParam(const GURL& url,
                             const std::string& application_locale);

// Returns the Google country code string for the given Google homepage URL.
// Returns an empty string if |google_homepage_url| contains no country code.
std::string GetGoogleCountryCode(const GURL& google_homepage_url);

// Returns the Google search URL for the given Google homepage URL.
GURL GetGoogleSearchURL(const GURL& google_homepage_url);

// Returns the Google base URL specified on the command line, if it exists.
// This performs some fixup and sanity-checking to ensure that the resulting URL
// is valid and has no query or ref.
const GURL& CommandLineGoogleBaseURL();

// Returns true if a Google base URL was specified on the command line and |url|
// begins with that base URL.  This uses a simple string equality check.
bool StartsWithCommandLineGoogleBaseURL(const GURL& url);

// WARNING: The following IsGoogleXXX() functions use heuristics to rule out
// "obviously false" answers.  They do NOT guarantee that the string in question
// is actually on a Google-owned domain, just that it looks plausible.

// Designate whether or not a URL checking function also checks for specific
// subdomains, or only "www" and empty subdomains.
enum SubdomainPermission {
  ALLOW_SUBDOMAIN,
  DISALLOW_SUBDOMAIN,
};

// Designate whether or not a URL checking function also checks for standard
// ports (80 for http, 443 for https), or if it allows all port numbers.
enum PortPermission {
  ALLOW_NON_STANDARD_PORTS,
  DISALLOW_NON_STANDARD_PORTS,
};

// True if |url| is a valid URL with a host that returns true for
// IsGoogleHostname(), and an HTTP or HTTPS scheme.  If |port_permission| is
// DISALLOW_NON_STANDARD_PORTS, this also requires |url| to use the standard
// port for its scheme (80 for HTTP, 443 for HTTPS).
//
// Note that this only checks for google.<TLD> domains, but not other Google
// properties. If you want to check domains including other Google properties,
// you can use IsGoogleAssociatedDomainUrl() below.
bool IsGoogleDomainUrl(const GURL& url,
                       SubdomainPermission subdomain_permission,
                       PortPermission port_permission);

// True if |host| is "[www.]google.<TLD>" with a valid TLD. If
// |subdomain_permission| is ALLOW_SUBDOMAIN, we check against host
// "*.google.<TLD>" instead. Note this function does not check the URL is an
// HTTP or HTTPS scheme. If checking a URL, use IsGoogleDomainUrl(). This
// function should only be used in cases when the input is just a hostname, such
// as a search engine keyword.
//
// If the Google base URL has been overridden on the command line, this function
// will also return true for any URL whose hostname exactly matches the hostname
// of the URL specified on the command line.  In this case,
// |subdomain_permission| is ignored.
bool IsGoogleHostname(std::string_view host,
                      SubdomainPermission subdomain_permission);

// True if |url| represents a valid Google home page URL.
bool IsGoogleHomePageUrl(const GURL& url);

// True if |url| represents a valid Google search URL.
bool IsGoogleSearchUrl(const GURL& url);

// True if |url| is a valid youtube.<TLD> URL.  If |port_permission| is
// DISALLOW_NON_STANDARD_PORTS, this also requires |url| to use the standard
// port for its scheme (80 for HTTP, 443 for HTTPS).
bool IsYoutubeDomainUrl(const GURL& url,
                        SubdomainPermission subdomain_permission,
                        PortPermission port_permission);

// True if |url| is hosted by Google.
bool IsGoogleAssociatedDomainUrl(const GURL& url);

// Returns the list of all Google's registerable domains, i.e. domains named
// google.<eTLD> owned by Google.
// TODO(msramek): This is currently only used to ensure the deletion of Google
// service workers on signout. Remove this once we have other options to do it,
// such as service workers discovering that signin cookies are missing and
// unregistering themselves.
const std::vector<std::string>& GetGoogleRegistrableDomains();

// Appends the provided |key| and |value| pair to the "async" query param list,
// according to the format used by the Google servers:
//
//   "async=<other key>:<other value>,<key>:<value>"
//
// If |url| does not have an "async" query param list it will be added.
// Derived from net::AppendOrReplaceQueryParameter, that can't be used because
// it escapes ":" to "%3A", but the servers requires the colon not to be
// escaped. See: http://crbug.com/413845.
GURL AppendToAsyncQueryParam(const GURL& url,
                             const std::string& key,
                             const std::string& value);

// Returns Google Search mode used by the user. This corresponds to the tab
// (e.g. web result, image results, video results, etc.) the user is on. This
// information is extracted from the "tbm" query parameter on the Search URL.
GoogleSearchMode GoogleSearchModeFromUrl(const GURL& url);

}  // namespace google_util

#endif  // COMPONENTS_GOOGLE_CORE_COMMON_GOOGLE_UTIL_H_
