// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_EVENT_CONSTANTS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_EVENT_CONSTANTS_H_

#include "build/build_config.h"

namespace feature_engagement {

namespace events {

// Desktop and IOS.
#if defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
// The user has explicitly opened a new tab via an entry point from inside of
// Chrome.
extern const char kNewTabOpened[];
#endif  // defined(OS_WIN) || defined(OS_APPLE) ||
        // defined(OS_LINUX) || defined(OS_CHROMEOS)

// Desktop
#if defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) || \
    defined(OS_CHROMEOS)
// A new tab was opened when 5 (or more) tabs were already open.
extern const char kSixthTabOpened[];
// The user made a new tab group.
extern const char kTabGroupCreated[];

// All conditions for reopen closed tab IPH were met. Since this IPH needs to
// track user events (opening/closing tabs, focusing the omnibox, etc) on the
// second level, it must be done manually.
extern const char kReopenTabConditionsMet[];
// The user reopened a previously closed tab.
extern const char kTabReopened[];

// A tab with playing media was sent to the background.
extern const char kMediaBackgrounded[];

// The user opened the Global Media Controls dialog.
extern const char kGlobalMediaControlsOpened[];

// All the events declared below are the string names of deferred onboarding
// events for the Focus Mode feature.

// The user has opened a Focus Mode window.
extern const char kFocusModeOpened[];
// All conditions for show Focus Mode IPH were met.
extern const char kFocusModeConditionsMet[];

// The WebUI tab strip was closed by the user.
extern const char kWebUITabStripClosed[];
// The WebUI tab strip was opened by the user.
extern const char kWebUITabStripOpened[];

#endif  // defined(OS_WIN) || defined(OS_APPLE) || defined(OS_LINUX) ||
        // defined(OS_CHROMEOS)

#if defined(OS_IOS)
// The user has opened Chrome (cold start or from background).
extern const char kChromeOpened[];

// The user has opened an incognito tab.
extern const char kIncognitoTabOpened[];

// The user has cleared their browsing data.
extern const char kClearedBrowsingData[];

// The user has viewed their reading list.
extern const char kViewedReadingList[];

// The user has triggered the translate infobar manually.
extern const char kTriggeredTranslateInfobar[];

// The user has viewed the the BottomToolbar tip.
extern const char kBottomToolbarOpened[];

// The Discover feed has loaded content in the NTP.
extern const char kDiscoverFeedLoaded[];
#endif  // defined(OS_IOS)

}  // namespace events

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_EVENT_CONSTANTS_H_
