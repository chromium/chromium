// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace {

const char kOsScheme[] = "os";
const char kOsUrlPrefix[] = "os://";
const char kChromeUIScheme[] = "chrome";
const char kChromeUrlPrefix[] = "chrome://";

// The start of the host portion of a GURL which starts with the os scheme.
const size_t kHostStart = sizeof(kOsUrlPrefix) - 1;

// Used for sanitation - any of the characters will cut the rest of the URL.
const char kTerminatingCharacters[] = "/\\? #.%$&*<>";

}  // namespace

namespace crosapi {

namespace gurl_os_handler_utils {

GURL SanitizeAshURL(const GURL& url) {
  if (!IsAshOsUrl(url)) {
    // We do not use url.GetWithEmptyPath() here as there are various cases
    // which do not result in proper results (e.g. chrome://flags => "" or
    // chrome://flags/123 => chrome://flags/).
    return url.DeprecatedGetOriginAsURL();
  }

  // Only keep the scheme and the host. Everything else gets cut off.
  base::StringPiece url_spec = url.spec();

  // Find the first character after the host start
  std::size_t host_end =
      url_spec.find_first_of(kTerminatingCharacters, kHostStart);

  // Note: We want to treat the internal URLs caseinsensitive. GURL usually
  // handles this - but as we use an unknown scheme, only the scheme is treated
  // caseinsensitive.
  if (host_end == std::string::npos)
    return GURL(base::ToLowerASCII(url_spec));

  return GURL(base::ToLowerASCII(url_spec.substr(0, host_end)));
}

bool IsUrlInList(const GURL& test_url, std::vector<GURL> list) {
  // It is assumed that the provided URL is sanitized as requested by
  // security at this point.
  DCHECK(SanitizeAshURL(test_url) == test_url);

  const GURL& target_url = test_url;
  for (const GURL& url : list) {
    // It is expected that all os:// scheme items in the list are lower case
    // no "/" at the end and properly sanitized as the GURL comparison will
    // treat everything past the unknown scheme as an unknown string and hence
    // do no processing.
    DCHECK(!IsAshOsUrl(url) || SanitizeAshURL(url) == url);
    if (target_url == url)
      return true;
  }
  return false;
}

bool IsAshOsUrl(const GURL& url) {
  if (!url.is_valid() || url.spec().length() <= strlen(kOsUrlPrefix))
    return false;

  // Do not use "url.SchemeIs(kOsScheme)" here as it would require further
  // parsing of the "://" portion to see if the Url is also valid.
  return base::StartsWith(url.spec(), kOsUrlPrefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsAshOsAsciiScheme(const base::StringPiece& scheme) {
  return base::LowerCaseEqualsASCII(scheme, kOsScheme);
}

std::string AshOsUrlHost(const GURL& url) {
  if (!url.is_valid())
    return "";

  // If we are using a default scheme, GURL does all for us.
  if (!IsAshOsUrl(url))
    return url.host();

  // Get the URL string after the start of the host portion.
  std::string url_part = url.spec().substr(kHostStart);

  // Find the first "invalid character" we want to cut off.
  std::size_t cut_off = url_part.find_first_of(kTerminatingCharacters);

  // Note: We want to treat the internal URLs caseinsensitive. GURL usually
  // handles this - but as we use an unknown scheme, only the scheme is treated
  // caseinsensitive.
  // Return the string to the terminating character - or all.
  if (cut_off == std::string::npos)
    return base::ToLowerASCII(url_part);

  return base::ToLowerASCII(url_part.substr(0, cut_off));
}

// Convert a passed GURL from os:// to chrome://.
GURL GetSystemUrlFromChromeUrl(const GURL& url) {
  DCHECK(url.SchemeIs(kChromeUIScheme));
  return GURL(kOsUrlPrefix + url.host());
}

// Convert a passed GURL from chrome:// to os://.
GURL GetChromeUrlFromSystemUrl(const GURL& url) {
  DCHECK(IsAshOsUrl(url));
  return GURL(kChromeUrlPrefix + AshOsUrlHost(url));
}

}  // namespace gurl_os_handler_utils

}  // namespace crosapi
