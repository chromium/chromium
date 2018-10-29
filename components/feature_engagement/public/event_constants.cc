// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/event_constants.h"

#include "build/build_config.h"
#include "components/feature_engagement/buildflags.h"

namespace feature_engagement {

namespace events {

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
const char kBookmarkAdded[] = "bookmark_added";
const char kBookmarkSessionTimeMet[] = "bookmark_session_time_met";

const char kOmniboxInteraction[] = "omnibox_used";
const char kNewTabSessionTimeMet[] = "new_tab_session_time_met";

const char kIncognitoWindowOpened[] = "incognito_window_opened";
const char kIncognitoWindowSessionTimeMet[] =
    "incognito_window_session_time_met";

const char kReopenTabConditionsMet[] = "reopen_tab_conditions_met";
const char kTabReopened[] = "tab_reopened";
#endif  // BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_IOS)
const char kNewTabOpened[] = "new_tab_opened";
#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_IOS)

#if defined(OS_IOS)
const char kChromeOpened[] = "chrome_opened";
const char kIncognitoTabOpened[] = "incognito_tab_opened";
const char kClearedBrowsingData[] = "cleared_browsing_data";
const char kViewedReadingList[] = "viewed_reading_list";
const char kBottomToolbarOpened[] = "bottom_toolbar_opened";
#endif  // defined(OS_IOS)

}  // namespace events

}  // namespace feature_engagement
