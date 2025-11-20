// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_ICON_TYPE_H_
#define CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_ICON_TYPE_H_

// Used for histograms, do not reorder. When adding to this enum, please also
// modify the corresponding references in
// tools/metrics/histograms/metadata/page/enums.xml and
// tools/metrics/histograms/metadata/page/histograms.xml and add a static assert
// below.
//
// LINT.IfChange(PageActionIconType)
enum class PageActionIconType {
  kBookmarkStar = 0,
  kClickToCall = 1,
  kCookieControls = 2,
  kFileSystemAccess = 3,
  kFind = 4,
  kMemorySaver = 5,
  kIntentPicker = 6,
  // DEPRECATED: kLocalCardMigration = 7,
  kManagePasswords = 8,
  kPaymentsOfferNotification = 9,
  kPriceTracking = 10,
  kPwaInstall = 11,
  // DEPRECATED: kQRCodeGenerator = 12,
  // DEPRECATED: kReaderMode = 13,
  kAutofillAddress = 14,
  kSaveCard = 15,
  // DEPRECATED: kSendTabToSelf = 16,
  kSharingHub = 17,
  // DEPRECATED: kSideSearch = 18,
  // DEPRECATED: kSmsRemoteFetcher = 19,
  kTranslate = 20,
  kVirtualCardEnroll = 21,
  kFilledCardInformation = 22,
  kZoom = 23,
  kSaveIban = 24,
  kMandatoryReauth = 25,
  kPriceInsights = 26,
  // DEPRECATED: kReadAnything = 27,
  kProductSpecifications = 28,
  kLensOverlay = 29,
  kDiscounts = 30,
  kOptimizationGuide = 31,
  kCollaborationMessaging = 32,
  // DEPRECATED: kChangePassword = 33,
  kLensOverlayHomework = 34,
  kAiMode = 35,
  kReadingMode = 36,
  kContextualSidePanel = 37,
  kJsOptimizations = 38,
  kMaxValue = kJsOptimizations,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:PageActionIconType)

static_assert(static_cast<int>(PageActionIconType::kBookmarkStar) == 0);
static_assert(static_cast<int>(PageActionIconType::kClickToCall) == 1);
static_assert(static_cast<int>(PageActionIconType::kCookieControls) == 2);
static_assert(static_cast<int>(PageActionIconType::kFileSystemAccess) == 3);
static_assert(static_cast<int>(PageActionIconType::kFind) == 4);
static_assert(static_cast<int>(PageActionIconType::kMemorySaver) == 5);
static_assert(static_cast<int>(PageActionIconType::kIntentPicker) == 6);
static_assert(static_cast<int>(PageActionIconType::kManagePasswords) == 8);
static_assert(
    static_cast<int>(PageActionIconType::kPaymentsOfferNotification) == 9);
static_assert(static_cast<int>(PageActionIconType::kPriceTracking) == 10);
static_assert(static_cast<int>(PageActionIconType::kPwaInstall) == 11);
static_assert(static_cast<int>(PageActionIconType::kAutofillAddress) == 14);
static_assert(static_cast<int>(PageActionIconType::kSaveCard) == 15);
static_assert(static_cast<int>(PageActionIconType::kSharingHub) == 17);
static_assert(static_cast<int>(PageActionIconType::kTranslate) == 20);
static_assert(static_cast<int>(PageActionIconType::kVirtualCardEnroll) == 21);
static_assert(static_cast<int>(PageActionIconType::kFilledCardInformation) ==
              22);
static_assert(static_cast<int>(PageActionIconType::kZoom) == 23);
static_assert(static_cast<int>(PageActionIconType::kSaveIban) == 24);
static_assert(static_cast<int>(PageActionIconType::kMandatoryReauth) == 25);
static_assert(static_cast<int>(PageActionIconType::kPriceInsights) == 26);
static_assert(static_cast<int>(PageActionIconType::kProductSpecifications) ==
              28);
static_assert(static_cast<int>(PageActionIconType::kLensOverlay) == 29);
static_assert(static_cast<int>(PageActionIconType::kDiscounts) == 30);
static_assert(static_cast<int>(PageActionIconType::kOptimizationGuide) == 31);
static_assert(static_cast<int>(PageActionIconType::kCollaborationMessaging) ==
              32);
static_assert(static_cast<int>(PageActionIconType::kLensOverlayHomework) == 34);
static_assert(static_cast<int>(PageActionIconType::kAiMode) == 35);
static_assert(static_cast<int>(PageActionIconType::kReadingMode) == 36);
static_assert(static_cast<int>(PageActionIconType::kContextualSidePanel) == 37);
static_assert(static_cast<int>(PageActionIconType::kJsOptimizations) == 38);

// Returns a bool indicating whether the given page action type has been
// migrated to the new framework, which is based on ActionItems instead of
// PageActionIconType.
bool IsPageActionMigrated(PageActionIconType page_action);

#endif  // CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_ICON_TYPE_H_
