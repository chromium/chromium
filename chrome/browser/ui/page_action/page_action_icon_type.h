// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_ICON_TYPE_H_
#define CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_ICON_TYPE_H_

// Used for histograms, do not reorder. When adding to this enum, please also
// modify the corresponding references in
// tools/metrics/histograms/metadata/page/enums.xml and
// tools/metrics/histograms/metadata/page/histograms.xml.
//
// LINT.IfChange(PageActionIconType)
enum class PageActionIconType {
  kBookmarkStar = 0,
  // DEPRECATED: kClickToCall = 1,
  kCookieControls = 2,
  kFileSystemAccess = 3,
  kFind = 4,
  kMemorySaver = 5,
  kIntentPicker = 6,
  // DEPRECATED: kLocalCardMigration = 7,
  kManagePasswords = 8,
  kPaymentsOfferNotification = 9,
  // DEPRECATED: kPriceTracking = 10,
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
  // DEPRECATED: kProductSpecifications = 28,
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
  kRecordReplay = 39,
  kIndigo = 40,
  kFederation = 41,
  kGlic = 42,
  kAnchoredContextualCue = 43,
  kWebAuthnAmbientSignin = 44,
  kAutofillPayment = 45,
  kMaxValue = kAutofillPayment,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/page/enums.xml:PageActionIconType)

// Returns a bool indicating whether the given page action type has been
// migrated to the new framework, which is based on ActionItems instead of
// PageActionIconType.
bool IsPageActionMigrated(PageActionIconType page_action);

#endif  // CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_ICON_TYPE_H_
