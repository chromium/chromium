// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement/public/event_constants.h"

#include "build/build_config.h"

namespace feature_engagement {

namespace events {

#if defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
const char kNewTabOpened[] = "new_tab_opened";
#endif  // defined(OS_WIN) || defined(OS_APPLE) ||
        // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
const char kSixthTabOpened[] = "sixth_tab_opened";
const char kTabGroupCreated[] = "tab_group_created";

const char kReopenTabConditionsMet[] = "reopen_tab_conditions_met";
const char kTabReopened[] = "tab_reopened";

const char kMediaBackgrounded[] = "media_backgrounded";
const char kGlobalMediaControlsOpened[] = "global_media_controls_opened";

const char kFocusModeOpened[] = "focus_mode_opened";
const char kFocusModeConditionsMet[] = "focus_mode_conditions_met";

const char kWebUITabStripClosed[] = "webui_tab_strip_closed";
const char kWebUITabStripOpened[] = "webui_tab_strip_opened";
#endif  // defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#if defined(OS_IOS)
const char kChromeOpened[] = "chrome_opened";
const char kIncognitoTabOpened[] = "incognito_tab_opened";
const char kClearedBrowsingData[] = "cleared_browsing_data";
const char kViewedReadingList[] = "viewed_reading_list";
const char kTriggeredTranslateInfobar[] = "triggered_translate_infobar";
const char kBottomToolbarOpened[] = "bottom_toolbar_opened";
const char kDiscoverFeedLoaded[] = "discover_feed_loaded";
#endif  // defined(OS_IOS)

}  // namespace events

}  // namespace feature_engagement
