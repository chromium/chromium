// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_CLIENT_NAMESPACE_CONSTANTS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_CLIENT_NAMESPACE_CONSTANTS_H_

#include "build/build_config.h"

namespace offline_pages {

// Currently used for fallbacks like tests.
extern const char kDefaultNamespace[];

// Any changes to these well-known namespaces should also be reflected in
// OfflinePagesNamespaceEnumeration (histograms.xml) for consistency.
// New namespaces should be put at the end of this list and a corresponding
// enum value should be added in OfflinePagesNamespaceEnumeration.
extern const char kBookmarkNamespace[];
extern const char kLastNNamespace[];
extern const char kAsyncNamespace[];
extern const char kCCTNamespace[];
extern const char kDownloadNamespace[];
extern const char kNTPSuggestionsNamespace[];
extern const char kBrowserActionsNamespace[];
extern const char kLivePageSharingNamespace[];
extern const char kAutoAsyncNamespace[];

// Enum of namespaces used by metric collection.
// See OfflinePagesNamespaceEnumeration in enums.xml for histogram usages.
// Changes to this enum should be in sync with the changes to the namespace
// constants above and with the metrics enum.
enum class OfflinePagesNamespaceEnumeration {
  DEFAULT = 0,
  BOOKMARK = 1,
  LAST_N = 2,
  ASYNC_LOADING = 3,
  CUSTOM_TABS = 4,
  DOWNLOAD = 5,
  NTP_SUGGESTION = 6,
  DEPRECATED_SUGGESTED_ARTICLES = 7,
  BROWSER_ACTIONS = 8,
  LIVE_PAGE_SHARING = 9,
  ASYNC_AUTO_LOADING = 10,
  kMaxValue = ASYNC_AUTO_LOADING,
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_CLIENT_NAMESPACE_CONSTANTS_H_
