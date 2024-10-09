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

// A side panel has been pinned.
extern const char kSidePanelPinned[];

// The side search panel was automatically triggered.
extern const char kSideSearchAutoTriggered[];
// The side search panel was opened by the user.
extern const char kSideSearchOpened[];
// The side search page action icon label was shown.
extern const char kSideSearchPageActionLabelShown[];

// Tab Search tab strip was opened by the user.
extern const char kTabSearchOpened[];

// The PWA was installed by the user.
extern const char kDesktopPwaInstalled[];

// A module's actions were clicked on the NewTabPage.
extern const char kDesktopNTPModuleUsed[];

// The user entered the special "focus help bubble" accelerator.
extern const char kFocusHelpBubbleAcceleratorPressed[];

// The screen reader promo for the "focus help bubble" accelerator was read to
// the user.
extern const char kFocusHelpBubbleAcceleratorPromoRead[];

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

// The user tapped Remind Me Later on a default browser promo.
extern const char kDefaultBrowserPromoRemindMeLater[];

// The Password Manager widget promo was triggered.
extern const char kPasswordManagerWidgetPromoTriggered[];

// The Password Manager widget was used.
extern const char kPasswordManagerWidgetPromoUsed[];

// The Password Manager widget promo was closed.
extern const char kPasswordManagerWidgetPromoClosed[];

// The user has been shown the blue dot default browser promo on the overflow
// carousel.
extern const char kBlueDotPromoOverflowMenuShown[];

// The user has been shown the blue dot default browser promo on the settings
// row.
extern const char kBlueDotPromoSettingsShown[];

// The user has opened the overflow menu while the blue dot was showing.
extern const char kBlueDotPromoOverflowMenuOpened[];

// The user has dismissed the blue dot default browser promo on the settings
// row.
extern const char kBlueDotPromoSettingsDismissed[];

// The user has customized the overflow menu while default browser blue dot was
// showing.
extern const char kBlueDotOverflowMenuCustomized[];

// The user has dismissed the blue dot default browser promo on the overflow
// carousel.
extern const char kBlueDotPromoOverflowMenuDismissed[];

// The user snoozed the Credential Provider Extension Promo.
extern const char kCredentialProviderExtensionPromoSnoozed[];

// The user tapped Remind Me Later on the Docking Promo.
extern const char kDockingPromoRemindMeLater[];

// The user opened an url from omnibox.
extern const char kOpenUrlFromOmnibox[];

// The history item on overflow menu is used.
extern const char kHistoryOnOverflowMenuUsed[];

// The user has triggered the Lens button in the Omnibox keyboard.
extern const char kLensButtonKeyboardUsed[];

// The user has triggered Parcel Tracking.
extern const char kParcelTrackingTriggered[];

// The user has tracked a parcel.
extern const char kParcelTracked[];

// The user has more than one gesture to refresh a page in iOS. This includes
// but not limited to re-typing the URL in omnibox and refreshing from context
// menu.
extern const char kIOSMultiGestureRefreshUsed[];

// The user has used the pull-to-refresh feature in iOS.
extern const char kIOSPullToRefreshUsed[];

// The user has tapped the dismiss button of the pull-to-refresh IPH.
extern const char kIOSPullToRefreshIPHDismissButtonTapped[];

// The user has tapped "incognito" on the page control in the tab grid.
extern const char kIOSIncognitoPageControlTapped[];

// The user has swiped right from regular tab grid to the incognito tab grid.
extern const char kIOSSwipeRightForIncognitoUsed[];

// The user has tapped the dismiss button of the "swipe right for incognito"
// IPH.
extern const char kIOSSwipeRightForIncognitoIPHDismissButtonTapped[];

// The user has tapped the toolbar backward/forward button to navigate on a tab.
extern const char kIOSBackForwardButtonTapped[];

// The user has swiped from the edge to navigate backward or forward on a tab.
extern const char kIOSSwipeBackForwardUsed[];

// The user has tapped the dismiss button of the "swipe to go back/forward" IPH.
extern const char kIOSSwipeBackForwardIPHDismissButtonTapped[];

// The user has tapped on an adjacent tab in the tab grid.
extern const char kIOSTabGridAdjacentTabTapped[];

// The user has swipped the toolbar to go to an adjacent tab.
extern const char kIOSSwipeToolbarToChangeTabUsed[];

// The user has tapped the dismiss button of the "swipe the toolbar to go to
// adjacent tab" IPH.
extern const char kIOSSwipeToolbarToChangeTabIPHDismissButtonTapped[];

// The user has opened the Overflow Menu customization screen.
extern const char kIOSOverflowMenuCustomizationUsed[];

// The user has used ann Overflow Menu item where customizing the menu could
// have helped.
extern const char kIOSOverflowMenuOffscreenItemUsed[];

// The Default Browser FRE promo was shown to the user.
extern const char kIOSDefaultBrowserFREShown[];

// The user has met all the conditions to be eligible for generic default
// browser promo.
extern const char kGenericDefaultBrowserPromoConditionsMet[];

// The user has met all the conditions to be eligible for All Tabs default
// browser promo.
extern const char kAllTabsPromoConditionsMet[];

// The user has met all the conditions to be eligible for Made for iOS default
// browser promo.
extern const char kMadeForIOSPromoConditionsMet[];

// The user has met all the conditions to be eligible for Stay Safe default
// browser promo.
extern const char kStaySafePromoConditionsMet[];

// The user has met a condition that makes the Enhanced Safe Browsing
// inline promos eligible to be displayed.
extern const char kEnhancedSafeBrowsingPromoCriterionMet[];

// The user taps the 'x' button on the Enhanced Safe Browsing inline promo.
extern const char kInlineEnhancedSafeBrowsingPromoClosed[];

// The generic default browser promo was triggered.
extern const char kGenericDefaultBrowserPromoTrigger[];

// The all tabs default browser promo was triggered.
extern const char kAllTabsPromoTrigger[];

// The made for iOS default browser promo was triggered.
extern const char kMadeForIOSPromoTrigger[];

// The stay safe default browser promo was triggered.
extern const char kStaySafePromoTrigger[];

// The tailored default browser promo group was triggered.
extern const char kTailoredDefaultBrowserPromosGroupTrigger[];

// The user has met the conditions for default browser trigger criteria
// experiment.
extern const char kDefaultBrowserPromoTriggerCriteriaConditionsMet[];

// The user has tapped the contextual panel entrypoint when it was showing the
// sample model info.
extern const char kIOSContextualPanelSampleModelEntrypointUsed[];

// The user has tapped the contextual panel entrypoint when it was branded with
// price insights infoblock.
extern const char kIOSContextualPanelPriceInsightsEntrypointUsed[];

// The user has explicitly dismissed the Price Insights branded Contextual Panel
// entrypoint in-product help.
extern const char
    kIOSContextualPanelPriceInsightsEntrypointExplicitlyDismissed[];

// The user has tapped the Home customization menu's entrypoint.
extern const char kHomeCustomizationMenuUsed[];

// The user has tapped on the lens overlay entrypoint.
extern const char kLensOverlayEntrypointUsed[];

#endif  // BUILDFLAG(IS_IOS)

// Android.
#if BUILDFLAG(IS_ANDROID)
// The user has explicitly used the Install menu item under the App Menu.
extern const char kPwaInstallMenuSelected[];
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace events

}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_PUBLIC_EVENT_CONSTANTS_H_
