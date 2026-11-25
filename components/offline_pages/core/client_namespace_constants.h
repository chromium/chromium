// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_CLIENT_NAMESPACE_CONSTANTS_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_CLIENT_NAMESPACE_CONSTANTS_H_

#include "build/build_config.h"

namespace offline_pages {

// Currently used for fallbacks like tests.
extern const char kDefaultNamespace[];

extern const char kBookmarkNamespace[];
extern const char kLastNNamespace[];
extern const char kAsyncNamespace[];
extern const char kCCTNamespace[];
extern const char kDownloadNamespace[];
extern const char kNTPSuggestionsNamespace[];
extern const char kBrowserActionsNamespace[];
extern const char kLivePageSharingNamespace[];
extern const char kAutoAsyncNamespace[];

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_CLIENT_NAMESPACE_CONSTANTS_H_
