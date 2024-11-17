// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef COMPONENTS_BROWSING_DATA_CORE_COOKIE_OR_CACHE_DELETION_CHOICE_H_
#define COMPONENTS_BROWSING_DATA_CORE_COOKIE_OR_CACHE_DELETION_CHOICE_H_

namespace browsing_data {

// A helper enum to report the deletion of cookies and/or cache. Do not
// reorder the entries, as this enum is passed to UMA.
//
// A Java counterpart will be generated for this enum so that it can be
// logged on Android.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.browsing_data
//
// LINT.IfChange(CookieOrCacheDeletionChoice)
enum class CookieOrCacheDeletionChoice {
  kNeitherCookiesNorCache = 0,
  kOnlyCookies = 1,
  kOnlyCache = 2,
  kBothCookiesAndCache = 3,
  kMaxValue = kBothCookiesAndCache,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/history/enums.xml:CookieOrCacheDeletion)

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CORE_COOKIE_OR_CACHE_DELETION_CHOICE_H_
