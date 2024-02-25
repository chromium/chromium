// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_COMMON_STRING_UTIL_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_COMMON_STRING_UTIL_H_

#include "base/time/time.h"
#include "base/values.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

// This namespace contains shared functionality for manipulating the strings
// and string resources in security error pages.
namespace security_interstitials::common_string_util {

// Returns the |gurl| as a URL appropriate for display in an error page.
std::u16string GetFormattedHostName(const GURL& gurl);

// For SSL-related errors that share a basic structure.
void PopulateSSLLayoutStrings(int cert_error,
                              base::Value::Dict& load_time_data);

// For SSL-related errors that provide debugging information.
void PopulateSSLDebuggingStrings(const net::SSLInfo ssl_info,
                                 const base::Time time_triggered,
                                 base::Value::Dict& load_time_data);

}  // namespace security_interstitials::common_string_util

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_COMMON_STRING_UTIL_H_
