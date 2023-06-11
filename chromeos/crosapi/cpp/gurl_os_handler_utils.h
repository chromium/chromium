// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_GURL_OS_HANDLER_UTILS_H_
#define CHROMEOS_CROSAPI_CPP_GURL_OS_HANDLER_UTILS_H_

#include <vector>

#include "url/gurl.h"

// Adds utility functions for Lacros's system URL handling.
//
// Lacros is sending some special system os:// URLs to Ash.
// The system OS URLs will be (only) detected in the omnibox and directly sent
// to Ash where they get converted into chrome:// URLs for navigation.
// Note:
//   - These utility functions do add the functionality to handle the os://
//     scheme without registering "os" as scheme as this should not be treated
//     as a general scheme. With that, we cannot rely on the GURL / url library
//     as it will not parse the URL and all functions will not work.
//   - As an unknown scheme, it will be treated case insensitive whereas the
//     host and rest of the URL is unprocessed and therefore case sensitive.
//   - The sanitization will take care of lower casing the host of the os://
//     URLs and cutting off anything which is not scheme, host or sub-host,
//     cutting off the terminating "/" sign if it exists.

namespace crosapi {

namespace gurl_os_handler_utils {

// Sanitize the URL according to security requests (only scheme, host and
// path). The path can also be removed if |include_path| is false.
// Example:
// chrome://settings/network?query would return
//       |include_path| false: chrome://settings/
//       |include_path| true:  chrome://settings/network
COMPONENT_EXPORT(CROSAPI)
GURL SanitizeAshURL(const GURL& url, bool include_path = true);

// Get the URL which should be used by Ash from a URL passed in by Lacros.
COMPONENT_EXPORT(CROSAPI)
GURL GetTargetURLFromLacrosURL(const GURL& url);

// Determines if a given URL matches any of the given URLs in the list.
// Note that the provided |url| needs to be sanitized.
// Note furthermore that the passed |list| is expected to be lower case for
// os:// scheme links.
COMPONENT_EXPORT(CROSAPI)
bool IsUrlInList(const GURL& url, const std::vector<GURL>& list);

// Returns true when the URL is an internal os:// url. Note that we do need
// This support only for OpenURL Lacros to Ash and as such
// RegisterContentScheme and/or url::AddSecureScheme should be avoided.
COMPONENT_EXPORT(CROSAPI) bool IsAshOsUrl(const GURL& url);

// Returns true when the passed scheme string matches the "os" scheme.
COMPONENT_EXPORT(CROSAPI) bool IsAshOsAsciiScheme(const base::StringPiece& url);

// Convert a passed GURL from os:// scheme to chrome:// scheme.
COMPONENT_EXPORT(CROSAPI) GURL GetOsUrlFromChromeUrl(const GURL& url);

// Convert a passed GURL from chrome:// scheme to os:// scheme.
COMPONENT_EXPORT(CROSAPI) GURL GetChromeUrlFromOsUrl(const GURL& url);

}  // namespace gurl_os_handler_utils

}  // namespace crosapi

#endif  // CHROMEOS_CROSAPI_CPP_GURL_OS_HANDLER_UTILS_H_
