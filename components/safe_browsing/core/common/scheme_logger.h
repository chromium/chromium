// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_COMMON_SCHEME_LOGGER_H_
#define COMPONENTS_SAFE_BROWSING_CORE_COMMON_SCHEME_LOGGER_H_

#include "url/gurl.h"

namespace safe_browsing::scheme_logger {

// Non-comprehensive list of URL schemes found mentioned throughout the
// codebase. More can be added if useful.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class UrlScheme {
  kUnknown = 0,
  kAbout = 1,
  kBlob = 2,
  kContent = 3,
  kCid = 4,
  kData = 5,
  kFile = 6,
  kFileSystem = 7,
  kFtp = 8,
  kHttp = 9,
  kHttps = 10,
  kJavascript = 11,
  kMailTo = 12,
  kQuicTransport_Obsoleted = 13,
  kTel = 14,
  kUrn = 15,
  kUuidInPackage = 16,
  kWebcal = 17,
  kWs = 18,
  kWss = 19,
  kIsolatedApp = 20,
  kChromeNative = 21,
  kChromeSearch = 22,
  kDevTools = 23,
  kChromeError = 24,
  kChrome = 25,
  kChromeUntrusted = 26,
  kChromeGuest_Obsolete = 27,
  kViewSource = 28,
  kExternalFile = 29,
  kAndroidApp = 30,
  kGoogleChrome = 31,
  kAndroidWebviewVideoPoster = 32,
  kChromeDistiller = 33,
  kChromeExtension = 34,

  kMaxValue = 34
};

// Logs the scheme of the |url| to the |enum_histogram_name| histogram. If the
// scheme is not found in the hard-coded list of schemes, it will log unknown.
void LogScheme(const GURL& url, const std::string& enum_histogram_name);

}  // namespace safe_browsing::scheme_logger

#endif  // COMPONENTS_SAFE_BROWSING_CORE_COMMON_SCHEME_LOGGER_H_
