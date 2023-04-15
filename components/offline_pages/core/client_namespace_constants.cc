// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/client_namespace_constants.h"

namespace offline_pages {

// NOTE: When adding a namespace constant, you MUST add this as a suffix in
// //tools/metrics/histograms/histograms.xml or risk crashes due to DCHECKs.
const char kBookmarkNamespace[] = "bookmark";
const char kLastNNamespace[] = "last_n";
const char kAsyncNamespace[] = "async_loading";
const char kCCTNamespace[] = "custom_tabs";
const char kDownloadNamespace[] = "download";
const char kNTPSuggestionsNamespace[] = "ntp_suggestions";
const char kBrowserActionsNamespace[] = "browser_actions";
const char kLivePageSharingNamespace[] = "live_page_sharing";
const char kAutoAsyncNamespace[] = "auto_async_loading";

const char kDefaultNamespace[] = "default";

}  // namespace offline_pages
