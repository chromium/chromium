// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"

#include <stddef.h>

#include <string_view>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "url/url_util.h"

namespace {

// Note that GURL can't operate on the "os://" scheme as it is intentionally
// not a registered scheme.
const char kOsScheme[] = "os";
const char kOsUrlPrefix[] = "os://";
const char kChromeUrlPrefix[] = "chrome://";
const char kChromeUIOSSettingsHost[] = "os-settings";

// The start of the host portion of a GURL which starts with the os scheme.
const size_t kHostStart = sizeof(kOsUrlPrefix) - 1;

GURL NormalizeAshUrl(GURL url,
                     bool keep_path = false,
                     bool keep_query = false) {
  if (!url.is_valid() || !url.has_host()) {
    return GURL();
  }

  // Shortcut.
  if (!url.has_ref() && !url.has_username() && !url.has_password() &&
      (keep_query || !url.has_query()) && !url.has_port() &&
      (keep_path || !url.has_path())) {
    return url;
  }

  GURL::Replacements replacements;
  replacements.ClearPassword();
  if (!keep_path) {
    replacements.ClearPath();
  }
  replacements.ClearPort();
  if (!keep_query) {
    replacements.ClearQuery();
  }
  replacements.ClearRef();
  replacements.ClearUsername();
  return url.ReplaceComponents(replacements);
}

}  // namespace

namespace crosapi {

namespace gurl_os_handler_utils {

GURL SanitizeAshUrl(const GURL& url) {
  return NormalizeAshUrl(url, /*keep_path*/ true, /*keep_query*/ true);
}

GURL GetAshUrlFromLacrosUrl(GURL url) {
  const bool has_os_scheme = HasOsScheme(url);
  if (has_os_scheme) {
    url = GURL(kChromeUrlPrefix + url.spec().substr(kHostStart));
  }

  if (has_os_scheme && url.host() == "settings") {
    // Change os://settings/* into chrome://os-settings/* which will be the long
    // term home for our OS-settings.
    GURL::Replacements replacements;
    replacements.SetHostStr(kChromeUIOSSettingsHost);
    url = url.ReplaceComponents(replacements);
  }

  return url;
}

bool IsAshUrlInList(const GURL& url, const std::vector<GURL>& list) {
  return std::any_of(list.begin(), list.end(), [&](const GURL& elem) {
    DCHECK(elem.is_valid());
    DCHECK_EQ(elem, NormalizeAshUrl(elem));
    return elem == NormalizeAshUrl(url);
  });
}

bool HasOsScheme(const GURL& url) {
  if (!url.is_valid() || url.spec().length() <= strlen(kOsUrlPrefix))
    return false;

  // Do not use "url.SchemeIs(kOsScheme)" here as it would require further
  // parsing of the "://" portion to see if the Url is also valid.
  return base::StartsWith(url.spec(), kOsUrlPrefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsOsScheme(std::string_view scheme) {
  return base::EqualsCaseInsensitiveASCII(scheme, kOsScheme);
}

}  // namespace gurl_os_handler_utils

}  // namespace crosapi
