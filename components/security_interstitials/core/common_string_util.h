// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SECURITY_INTERSTITIALS_CORE_COMMON_STRING_UTIL_H_
#define COMPONENTS_SECURITY_INTERSTITIALS_CORE_COMMON_STRING_UTIL_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "base/values.h"
#include "net/ssl/ssl_info.h"
#include "url/gurl.h"

namespace security_interstitials {

// This namespace contains shared functionality for manipulating the strings
// and string resources in security error pages.
namespace common_string_util {

// Returns the |gurl| as a URL appropriate for display in an error page.
base::string16 GetFormattedHostName(const GURL& gurl);

// For SSL-related errors that share a basic structure.
void PopulateSSLLayoutStrings(int cert_error,
                              base::DictionaryValue* load_time_data);

// For SSL-related errors that provide debugging information.
void PopulateSSLDebuggingStrings(const net::SSLInfo ssl_info,
                                 const base::Time time_triggered,
                                 base::DictionaryValue* load_time_data);

// For determining whether to use the old or new icon sets.
void PopulateNewIconStrings(base::DictionaryValue* load_time_data);

// Populate a 'darkModeAvailable' boolean in |load_time_data| that specifies
// whether dark mode styling is available.
void PopulateDarkModeDisplaySetting(base::DictionaryValue* load_time_data);

}  // common_string_util

}  // namespace security_interstitials

#endif  // COMPONENTS_SECURITY_INTERSTITIALS_CORE_COMMON_STRING_UTIL_H_
