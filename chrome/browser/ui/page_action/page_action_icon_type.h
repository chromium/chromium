// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_ICON_TYPE_H_
#define CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_ICON_TYPE_H_

// Used for histograms, do not reorder. When adding to this enum, please also
// modify the corresponding references in tools/metrics/histograms/enums.xml
// and tools/metrics/histograms/metadata/page/histograms.xml and add a static
// assert below.
enum class PageActionIconType {
  kBookmarkStar = 0,
  kClickToCall,
  kCookieControls,
  kFileSystemAccess,
  kFind,
  kHighEfficiency,
  kIntentPicker,
  kLocalCardMigration,
  kManagePasswords,
  kPaymentsOfferNotification,
  kPriceTracking,
  kPwaInstall,
  kQRCodeGenerator,
  kReaderMode,
  kSaveAutofillAddress,
  kSaveCard,
  kSendTabToSelf,
  kSharingHub,
  kSideSearch,
  kSmsRemoteFetcher,
  kTranslate,
  kVirtualCardEnroll,
  kVirtualCardManualFallback,
  kZoom,
  kSaveIban,
  kMaxValue = kSaveIban,
};

static_assert(static_cast<int>(PageActionIconType::kBookmarkStar) == 0);
static_assert(static_cast<int>(PageActionIconType::kClickToCall) == 1);
static_assert(static_cast<int>(PageActionIconType::kCookieControls) == 2);
static_assert(static_cast<int>(PageActionIconType::kFileSystemAccess) == 3);
static_assert(static_cast<int>(PageActionIconType::kFind) == 4);
static_assert(static_cast<int>(PageActionIconType::kHighEfficiency) == 5);
static_assert(static_cast<int>(PageActionIconType::kIntentPicker) == 6);
static_assert(static_cast<int>(PageActionIconType::kLocalCardMigration) == 7);
static_assert(static_cast<int>(PageActionIconType::kManagePasswords) == 8);
static_assert(
    static_cast<int>(PageActionIconType::kPaymentsOfferNotification) == 9);
static_assert(static_cast<int>(PageActionIconType::kPriceTracking) == 10);
static_assert(static_cast<int>(PageActionIconType::kPwaInstall) == 11);
static_assert(static_cast<int>(PageActionIconType::kQRCodeGenerator) == 12);
static_assert(static_cast<int>(PageActionIconType::kReaderMode) == 13);
static_assert(static_cast<int>(PageActionIconType::kSaveAutofillAddress) == 14);
static_assert(static_cast<int>(PageActionIconType::kSaveCard) == 15);
static_assert(static_cast<int>(PageActionIconType::kSendTabToSelf) == 16);
static_assert(static_cast<int>(PageActionIconType::kSharingHub) == 17);
static_assert(static_cast<int>(PageActionIconType::kSideSearch) == 18);
static_assert(static_cast<int>(PageActionIconType::kSmsRemoteFetcher) == 19);
static_assert(static_cast<int>(PageActionIconType::kTranslate) == 20);
static_assert(static_cast<int>(PageActionIconType::kVirtualCardEnroll) == 21);
static_assert(
    static_cast<int>(PageActionIconType::kVirtualCardManualFallback) == 22);
static_assert(static_cast<int>(PageActionIconType::kZoom) == 23);
static_assert(static_cast<int>(PageActionIconType::kSaveIban) == 24);
#endif  // CHROME_BROWSER_UI_PAGE_ACTION_PAGE_ACTION_ICON_TYPE_H_
