// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_GURL_OS_HANDLER_UTILS_H_
#define CHROMEOS_CROSAPI_CPP_GURL_OS_HANDLER_UTILS_H_

#include <string_view>
#include <vector>

#include "url/gurl.h"

// Utility functions for handling Lacros's system URLs and converting them to
// Ash URLs.
//
// Lacros makes the syntax and semantics of internal URLs a bit more complex.
// Depending on context, a URL may use the os:// scheme to refer to pages that
// live in Ash. For example, such URLs are recognized in the ChromeOS launcher
// and in the Lacros omnibox. They are converted to regular chrome:// URLs
// before being sent to Ash for navigation. They are also sanitized by stripping
// away certain components.
//
// NOTE:
// The os://scheme is intentionally not registered as a proper scheme and hence
// we cannot rely on the GURL / url library for parsing and canonicalization.

namespace crosapi {

namespace gurl_os_handler_utils {

// Convert a Lacros URL to the corresponding Ash URL.
// Example: os://settings/network?query => chrome://os-settings/network?query
COMPONENT_EXPORT(CROSAPI)
GURL GetAshUrlFromLacrosUrl(GURL url);

// Sanitize an Ash URL by stripping away certain parts.
// Example: chrome://os-settings/network#ref => chrome://os-settings/network
COMPONENT_EXPORT(CROSAPI)
GURL SanitizeAshUrl(const GURL& url);

// Determines if a given Ash URL matches any of the given URLs in the list.
COMPONENT_EXPORT(CROSAPI)
bool IsAshUrlInList(const GURL& url, const std::vector<GURL>& list);

// Returns true when the URL is an internal os:// url.
COMPONENT_EXPORT(CROSAPI) bool HasOsScheme(const GURL& url);

// Returns true when the passed scheme string matches the "os" scheme.
COMPONENT_EXPORT(CROSAPI) bool IsOsScheme(std::string_view scheme);

}  // namespace gurl_os_handler_utils

}  // namespace crosapi

#endif  // CHROMEOS_CROSAPI_CPP_GURL_OS_HANDLER_UTILS_H_
