// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_EVENT_CONSTANTS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_EVENT_CONSTANTS_H_

#include "build/build_config.h"
#include "components/feature_engagement/buildflags.h"

namespace feature_engagement {

namespace events {

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
// All the events declared below are the string names of deferred onboarding
// events for the Bookmark feature.

// The user has added a Bookmark (one-off event).
extern const char kBookmarkAdded[];
// The user has satisfied the session time requirement to show the BookmarkPromo
// by accumulating 5 hours of active session time (one-off event).
extern const char kBookmarkSessionTimeMet[];

// All the events declared below are the string names of deferred onboarding
// events for the New Tab.

// The user has interacted with the omnibox.
extern const char kOmniboxInteraction[];
// The user has satisfied the session time requirement to show the NewTabPromo
// by accumulating 2 hours of active session time (one-off event).
extern const char kNewTabSessionTimeMet[];

// All the events declared below are the string names of deferred onboarding
// events for the Incognito Window.

// The user has opened an incognito window.
extern const char kIncognitoWindowOpened[];
// The user has satisfied the session time requirement to show the
// IncognitoWindowPromo by accumulating 2 hours of active session time (one-off
// event).
extern const char kIncognitoWindowSessionTimeMet[];

// All conditions for reopen closed tab IPH were met. Since this IPH needs to
// track user events (opening/closing tabs, focusing the omnibox, etc) on the
// second level, it must be done manually.
extern const char kReopenTabConditionsMet[];
// The user reopened a previously closed tab.
extern const char kTabReopened[];
#endif  // BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)

#if defined(OS_WIN) || defined(OS_LINUX) || defined(OS_IOS)
// This event is included in the deferred onboarding events for the New Tab
// described above, but it is also used on iOS, so it must be compiled
// separately.

// The user has explicitly opened a new tab via an entry point from inside of
// Chrome.
extern const char kNewTabOpened[];
#endif  // defined(OS_WIN) || defined(OS_LINUX) || defined(OS_IOS)

#if defined(OS_IOS)
// The user has opened Chrome (cold start or from background).
extern const char kChromeOpened[];

// The user has opened an incognito tab.
extern const char kIncognitoTabOpened[];

// The user has cleared their browsing data.
extern const char kClearedBrowsingData[];

// The user has viewed their reading list.
extern const char kViewedReadingList[];

// The user has viewed the the BottomToolbar tip.
extern const char kBottomToolbarOpened[];
#endif  // defined(OS_IOS)

}  // namespace events

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_EVENT_CONSTANTS_H_
