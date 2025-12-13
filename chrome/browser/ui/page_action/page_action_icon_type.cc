// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_action/page_action_icon_type.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"

namespace {

const base::FeatureParam<bool>* GetPageActionsMigrationParam(
    PageActionIconType page_action) {
  switch (page_action) {
    case PageActionIconType::kLensOverlay:
      return &features::kPageActionsMigrationLensOverlay;
    case PageActionIconType::kMemorySaver:
      return &features::kPageActionsMigrationMemorySaver;
    case PageActionIconType::kTranslate:
      return &features::kPageActionsMigrationTranslate;
    case PageActionIconType::kIntentPicker:
      return &features::kPageActionsMigrationIntentPicker;
    case PageActionIconType::kZoom:
      return &features::kPageActionsMigrationZoom;
    case PageActionIconType::kPaymentsOfferNotification:
      return &features::kPageActionsMigrationOfferNotification;
    case PageActionIconType::kFileSystemAccess:
      return &features::kPageActionsMigrationFileSystemAccess;
    case PageActionIconType::kPwaInstall:
      return &features::kPageActionsMigrationPwaInstall;
    case PageActionIconType::kPriceInsights:
      return &features::kPageActionsMigrationPriceInsights;
    case PageActionIconType::kDiscounts:
      return &features::kPageActionsMigrationDiscounts;
    case PageActionIconType::kManagePasswords:
      return &features::kPageActionsMigrationManagePasswords;
    case PageActionIconType::kCookieControls:
      return &features::kPageActionsMigrationCookieControls;
    case PageActionIconType::kAutofillAddress:
      return &features::kPageActionsMigrationAutofillAddress;
    case PageActionIconType::kFind:
      return &features::kPageActionsMigrationFind;
    case PageActionIconType::kCollaborationMessaging:
      return &features::kPageActionsMigrationCollaborationMessaging;
    case PageActionIconType::kPriceTracking:
      return &features::kPageActionsMigrationPriceTracking;
    case PageActionIconType::kMandatoryReauth:
      return &features::kPageActionsMigrationAutofillMandatoryReauth;
    case PageActionIconType::kClickToCall:
      return &features::kPageActionsMigrationClickToCall;
    case PageActionIconType::kSharingHub:
      return &features::kPageActionsMigrationSharingHub;
    case PageActionIconType::kAiMode:
      return &features::kPageActionsMigrationAiMode;
    case PageActionIconType::kVirtualCardEnroll:
      return &features::kPageActionsMigrationVirtualCard;
    case PageActionIconType::kFilledCardInformation:
      return &features::kPageActionsMigrationFilledCardInformation;
    case PageActionIconType::kReadingMode:
      return &features::kPageActionsMigrationReadingMode;
    case PageActionIconType::kSaveIban:
    case PageActionIconType::kSaveCard:
      return &features::kPageActionsMigrationSavePayments;
    case PageActionIconType::kLensOverlayHomework:
      return &features::kPageActionsMigrationLensOverlayHomework;
    case PageActionIconType::kBookmarkStar:
      return &features::kPageActionsMigrationBookmarkStar;
    default:
      return nullptr;
  }
}

}  // namespace

bool IsPageActionMigrated(PageActionIconType page_action) {
  if (!base::FeatureList::IsEnabled(features::kPageActionsMigration)) {
    return false;
  }

  // Page actions on the new framework that don't have an implementation on the legacy path
  // and don't have a feature param.
  if (page_action == PageActionIconType::kContextualSidePanel ||
      page_action == PageActionIconType::kJsOptimizations) {
    return true;
  }

  const auto* feature_param = GetPageActionsMigrationParam(page_action);
  if (feature_param == nullptr) {
    return false;
  }

  // For developer manual testing only, allow all migrated page actions to be
  // enabled through a single switch.
  if (features::kPageActionsMigrationEnableAll.Get()) {
    return true;
  }

  return feature_param->Get();
}
