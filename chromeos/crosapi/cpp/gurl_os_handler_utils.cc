// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "url/url_util.h"

namespace {

const char kOsScheme[] = "os";
const char kOsUrlPrefix[] = "os://";
const char kChromeUIScheme[] = "chrome";
const char kChromeUrlPrefix[] = "chrome://";
const char kOsUISettingsURL[] = "os://settings";
const char kChromeUIOSSettingsHost[] = "os-settings";

// The start of the host portion of a GURL which starts with the os scheme.
const size_t kHostStart = sizeof(kOsUrlPrefix) - 1;

// Used for sanitation - any of the characters will cut the rest of the URL.
const char kTerminatingCharacters[] = "/\\? #.%$&*<>+";

// Note that GURL can't operate on the "os://" scheme as it is intentionally
// not a registered scheme.
std::string GetValidHostAndSubhostFromOsUrl(const GURL& url,
                                            bool include_path) {
  // Only keep the scheme, host and sub-host. Everything else gets cut off.
  const std::string& url_spec = base::ToLowerASCII(url.spec());

  // Find the first character after the host start
  std::size_t valid_spec_end =
      url_spec.find_first_of(kTerminatingCharacters, kHostStart);

  if (valid_spec_end == std::string::npos)
    return url_spec.substr(kHostStart);

  if (url_spec[valid_spec_end] == '/' && include_path) {
    // A sub URL is allowed (e.g. chrome://settings/network) - so we skip the
    // "/" with +1 and parse till we find the next terminating character.
    const std::size_t sub_host_end =
        url_spec.find_first_of(kTerminatingCharacters, valid_spec_end + 1);

    if (sub_host_end == std::string::npos)
      return url_spec.substr(kHostStart);

    if (sub_host_end > valid_spec_end + 1)
      valid_spec_end = sub_host_end;
  }

  // Copy beginning from after "os://" all characters in host and sub-host.
  return url_spec.substr(kHostStart, valid_spec_end - kHostStart);
}

GURL GetValidHostAndSubhostFromGURL(GURL gurl, bool include_path) {
  if (!gurl.is_valid() || !gurl.has_host())
    return GURL();

  if (!gurl.has_ref() && !gurl.has_username() && !gurl.has_password() &&
      !gurl.has_query() && !gurl.has_port() &&
      (include_path || !gurl.has_path())) {
    return gurl;
  }

  GURL::Replacements replacements;
  if (!include_path)
    replacements.ClearPath();
  replacements.ClearRef();
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearPort();
  return gurl.ReplaceComponents(replacements);
}

}  // namespace

namespace crosapi {

namespace gurl_os_handler_utils {

GURL SanitizeAshURL(const GURL& url, bool include_path) {
  if (!IsAshOsUrl(url))
    return GetValidHostAndSubhostFromGURL(url, include_path);

  return GURL(kOsUrlPrefix +
              GetValidHostAndSubhostFromOsUrl(url, include_path));
}

GURL GetTargetURLFromLacrosURL(const GURL& url) {
  GURL target_url = crosapi::gurl_os_handler_utils::SanitizeAshURL(url);
  GURL short_target_url = crosapi::gurl_os_handler_utils::SanitizeAshURL(
      url, /*include_path=*/false);

  if (short_target_url != GURL(kOsUISettingsURL))
    return target_url;
  // Change os://settings/* into chrome://os-settings/* which will be the long
  // term home for our OS-settings.

  // This converts the os (GURL lib unusable) address into a chrome
  // (GURL lib usable) address.
  target_url =
      crosapi::gurl_os_handler_utils::GetChromeUrlFromSystemUrl(target_url);
  GURL::Replacements replacements;
  replacements.SetHostStr(kChromeUIOSSettingsHost);
  return target_url.ReplaceComponents(replacements);
}

bool IsUrlInList(const GURL& test_url, const std::vector<GURL>& list) {
  // It is assumed that the provided URL is sanitized as requested by
  // security at this point.
  DCHECK(SanitizeAshURL(test_url) == test_url);

  const GURL short_url = SanitizeAshURL(test_url, /*include_path=*/false);
  for (const GURL& url : list) {
    // It is expected that all os:// scheme items in the list are lower case
    // no "/" at the end and properly sanitized as the GURL comparison will
    // treat everything past the unknown scheme as an unknown string and hence
    // do no processing.
    DCHECK(!IsAshOsUrl(url) || SanitizeAshURL(url) == url);
    if (test_url == url || short_url == url)
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
  return base::EqualsCaseInsensitiveASCII(scheme, kOsScheme);
}

std::string AshOsUrlHost(const GURL& url) {
  if (!url.is_valid())
    return "";

  // If we are using a default scheme, GURL does all for us.
  if (!IsAshOsUrl(url)) {
    if (!url.has_host())
      return "";
    return url.host() + (url.path().length() > 1 ? url.path() : "");
  }

  return GetValidHostAndSubhostFromOsUrl(url, /*include_path=*/true);
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
