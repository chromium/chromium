// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_EVENT_CONSTANTS_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_EVENT_CONSTANTS_H_

#include "build/build_config.h"
#include "components/feature_engagement/public/feature_constants.h"

namespace feature_engagement {

namespace events {

// Desktop
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
// The user has explicitly opened a new tab via an entry point from inside of
// Chrome.
extern const char kNewTabOpened[];
// A new tab was opened when 5 (or more) tabs were already open.
extern const char kSixthTabOpened[];
// The user made a new tab group.
extern const char kTabGroupCreated[];
// A tab was closed when there are eight or more tabs in the browser.
extern const char kClosedTabWithEightOrMore[];
// A tab was added to reading list.
extern const char kReadingListItemAdded[];
// Reading list was opened.
extern const char kReadingListMenuOpened[];
// Bookmark star button was clicked opening the menu.
extern const char kBookmarkStarMenuOpened[];
// Customize chrome was opened.
extern const char kCustomizeChromeOpened[];

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

// The side search panel was automatically triggered.
extern const char kSideSearchAutoTriggered[];
// The side search panel was opened by the user.
extern const char kSideSearchOpened[];
// The side search page action icon label was shown.
extern const char kSideSearchPageActionLabelShown[];

// Tab Search tab strip was opened by the user.
extern const char kTabSearchOpened[];

// The WebUI tab strip was closed by the user.
extern const char kWebUITabStripClosed[];
// The WebUI tab strip was opened by the user.
extern const char kWebUITabStripOpened[];

// The PWA was installed by the user.
extern const char kDesktopPwaInstalled[];

// The user entered the special "focus help bubble" accelerator.
extern const char kFocusHelpBubbleAcceleratorPressed[];

// The screen reader promo for the "focus help bubble" accelerator was read to
// the user.
extern const char kFocusHelpBubbleAcceleratorPromoRead[];

// The user has opened the battery saver bubble dialog
extern const char kBatterySaverDialogShown[];

// The user has opened the high efficiency page action chip
extern const char kHighEfficiencyDialogShown[];

// The user clicked on the performance menu item
extern const char kPerformanceMenuItemActivated[];

// Extensions menu is opened when any extension has access to the current site.
extern const char kExtensionsMenuOpenedWhileExtensionHasAccess[];

// Th user clicked the extensions request access button in the toolbar.
extern const char kExtensionsRequestAccessButtonClicked[];

// The user has opened the cookie controls bubble.
extern const char kCookieControlsBubbleShown[];

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_IOS)
// The user has opened Chrome (cold start or from background).
extern const char kChromeOpened[];

// The user has opened an incognito tab.
extern const char kIncognitoTabOpened[];

// The user has cleared their browsing data.
extern const char kClearedBrowsingData[];

// The user has viewed their reading list.
extern const char kViewedReadingList[];

// The user has viewed What's New.
extern const char kViewedWhatsNew[];

// The user has triggered the translate infobar manually.
extern const char kTriggeredTranslateInfobar[];

// The user has viewed the BottomToolbar tip.
extern const char kBottomToolbarOpened[];

// The Discover feed has loaded content in the NTP.
extern const char kDiscoverFeedLoaded[];

// The user has requested the desktop version of a page.
extern const char kDesktopVersionRequested[];

// The default site view mode has been used.
extern const char kDefaultSiteViewUsed[];

// The user has exited the overflow menu without scrolling horizontally and
// without taking an action.
extern const char kOverflowMenuNoHorizontalScrollOrAction[];

// The user has opened Price Tracking.
extern const char kPriceNotificationsUsed[];

// The user has been shown a default browser promo.
extern const char kDefaultBrowserPromoShown[];

// The user has taken an action that is a criterion towards becoming eligible to
// be shown the blue dot default browser promo.
extern const char kBlueDotPromoCriterionMet[];

// The user has met all criteria and has become eligible to be shown the blue
// dot default browser promo.
extern const char kBlueDotPromoEligibilityMet[];

// The user has been shown the blue dot default browser promo on the overflow
// carousel.
extern const char kBlueDotPromoOverflowMenuShown[];

// The user has been shown the blue dot default browser promo on the overflow
// carousel, for a new user session. (i.e. after 6 hours from last shown).
extern const char kBlueDotPromoOverflowMenuShownNewSession[];

// The user has been shown the blue dot default browser promo on the settings
// row.
extern const char kBlueDotPromoSettingsShown[];

// The user has been shown the blue dot default browser promo on the settings
// row, after a new user session (i.e. after 6 hours from last shown).
extern const char kBlueDotPromoSettingsShownNewSession[];

// The user has dismissed the blue dot default browser promo on the overflow
// carousel.
extern const char kBlueDotPromoOverflowMenuDismissed[];

// The user has dismissed the blue dot default browser promo on the settings
// row.
extern const char kBlueDotPromoSettingsDismissed[];

// The user snoozed the Credential Provider Extension Promo
extern const char kCredentialProviderExtensionPromoSnoozed[];

// The user opened an url from omnibox.
extern const char kOpenUrlFromOmnibox[];

// The new tab toolbar item is used.
extern const char kNewTabToolbarItemUsed[];

// The tab grid toolbar item is used.
extern const char kTabGridToolbarItemUsed[];

// The history item on overflow menu is used.
extern const char kHistoryOnOverflowMenuUsed[];

// The share item on the toolbar is used.
extern const char kShareToolbarItemUsed[];

// The user has met a condition that makes the default browser video promo
// eligible to be displayed.
extern const char kDefaultBrowserVideoPromoConditionsMet[];

#endif  // BUILDFLAG(IS_IOS)

// Android.
#if BUILDFLAG(IS_ANDROID)
// The user has explicitly used the Install menu item under the App Menu.
extern const char kPwaInstallMenuSelected[];
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace events

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_EVENT_CONSTANTS_H_
