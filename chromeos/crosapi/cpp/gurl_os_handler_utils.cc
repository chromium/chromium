// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"

#include <stddef.h>

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

GURL NormalizeAshUrl(GURL gurl) {
  if (!gurl.is_valid() || !gurl.has_host()) {
    return GURL();
  }

  if (!gurl.has_ref() && !gurl.has_username() && !gurl.has_password() &&
      !gurl.has_query() && !gurl.has_port() && !gurl.has_path()) {
    return gurl;
  }

  GURL::Replacements replacements;
  replacements.ClearPassword();
  replacements.ClearPath();
  replacements.ClearPort();
  replacements.ClearQuery();
  replacements.ClearRef();
  replacements.ClearUsername();
  return gurl.ReplaceComponents(replacements);
}

}  // namespace

namespace crosapi {

namespace gurl_os_handler_utils {

// Like NormalizeAshUrl but keep path.
GURL SanitizeAshUrl(const GURL& gurl) {
  if (!gurl.is_valid() || !gurl.has_host()) {
    return GURL();
  }

  // Shortcut.
  if (!gurl.has_ref() && !gurl.has_username() && !gurl.has_password() &&
      !gurl.has_query() && !gurl.has_port()) {
    return gurl;
  }

  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearUsername();
  replacements.ClearPassword();
  replacements.ClearQuery();
  replacements.ClearPort();
  return gurl.ReplaceComponents(replacements);
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

bool IsAshUrlInList(const GURL& test_url, const std::vector<GURL>& list) {
  GURL normalized_test_url = NormalizeAshUrl(test_url);
  const bool normalized_match =
      std::any_of(list.begin(), list.end(), [&](const GURL& elem) {
        DCHECK(elem.is_valid());
        DCHECK_EQ(elem, SanitizeAshUrl(elem));
        return elem == normalized_test_url;
      });
  return normalized_match ||
         std::find(list.begin(), list.end(), SanitizeAshUrl(test_url)) !=
             std::end(list);
}

bool HasOsScheme(const GURL& url) {
  if (!url.is_valid() || url.spec().length() <= strlen(kOsUrlPrefix))
    return false;

  // Do not use "url.SchemeIs(kOsScheme)" here as it would require further
  // parsing of the "://" portion to see if the Url is also valid.
  return base::StartsWith(url.spec(), kOsUrlPrefix,
                          base::CompareCase::INSENSITIVE_ASCII);
}

bool IsOsScheme(const base::StringPiece& scheme) {
  return base::EqualsCaseInsensitiveASCII(scheme, kOsScheme);
}

}  // namespace gurl_os_handler_utils

}  // namespace crosapi
