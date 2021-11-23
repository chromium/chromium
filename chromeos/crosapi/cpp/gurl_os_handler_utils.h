// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROSAPI_CPP_GURL_OS_HANDLER_UTILS_H_
#define CHROMEOS_CROSAPI_CPP_GURL_OS_HANDLER_UTILS_H_

#include <vector>

#include "url/gurl.h"

#if defined(OS_CHROMEOS)

// Adds utility functions for Lacros's system URL handling.
//
// Lacros is sending some special system os URLs to Ash for execution. These
// utility functions do add the functionality to handle the os:// scheme without
// adding it to the list of known schemes by the GURL / url library.
//
// Note also that the unknown scheme is treated case insensitive whereas the
// host and rest of the URL is unprocessed and therefore case senseitive.
// The sanitization will take care of lower casing the host of the os:// urls
// as will these functions making sure that this is properly handled.
// The host() / remaining spec() of the URL will also not be terminated by a
// "/" sign.

namespace crosapi {

namespace gurl_os_handler_utils {

// Sanitize the URL according to security requests (only scheme and hos,
// nothing more should be passed). Note that as os:// is not safe to be added
// to the general GURL handler, we cannot use the standard library for this.
COMPONENT_EXPORT(CROSAPI) GURL SanitizeAshURL(const GURL& url);

// Determines if a given URL matches any of the given URLs in the list.
// Note that the provided |url| needs to be sanitized.
// Note furthermore that the passed |list| is expected to be lower case for
// os:// scheme links.
COMPONENT_EXPORT(CROSAPI)
bool IsUrlInList(const GURL& url, const std::vector<GURL> list);

// Returns true when the URL is an internal os:// url. Note that we do need
// This support only for OpenURL Lacros to Ash and as such
// RegisterContentScheme and/or url::AddSecureScheme should be avoided.
COMPONENT_EXPORT(CROSAPI) bool IsAshOsUrl(const GURL& url);

// Returns true when the passed scheme string matches the "os" scheme.
COMPONENT_EXPORT(CROSAPI) bool IsAshOsAsciiScheme(const base::StringPiece& url);

// Get the host from the given os:// URL.
COMPONENT_EXPORT(CROSAPI) std::string AshOsUrlHost(const GURL& url);

// Convert a passed GURL from os:// scheme to chrome:// scheme.
COMPONENT_EXPORT(CROSAPI) GURL GetSystemUrlFromChromeUrl(const GURL& url);

// Convert a passed GURL from chrome:// scheme to os:// scheme.
COMPONENT_EXPORT(CROSAPI) GURL GetChromeUrlFromSystemUrl(const GURL& url);

}  // namespace gurl_os_handler_utils

}  // namespace crosapi

#endif  // defined(OS_CHROMEOS)

#endif  // CHROMEOS_CROSAPI_CPP_GURL_OS_HANDLER_UTILS_H_
