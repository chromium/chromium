// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/page_action/page_action_icon_type.h"

#include "base/feature_list.h"
#include "chrome/browser/ui/ui_features.h"

bool IsPageActionMigrated(PageActionIconType page_action) {
  if (!base::FeatureList::IsEnabled(features::kPageActionsMigration)) {
    return false;
  }

  // Page actions on the new framework that don't have an implementation on the
  // legacy path and don't have a feature param.
  switch (page_action) {
    case PageActionIconType::kAnchoredContextualCue:
    case PageActionIconType::kCollaborationMessaging:
    case PageActionIconType::kGlic:
    case PageActionIconType::kLensOverlay:
    case PageActionIconType::kMemorySaver:
    case PageActionIconType::kTranslate:
    case PageActionIconType::kFind:
    case PageActionIconType::kPwaInstall:
    case PageActionIconType::kAutofillAddress:
    case PageActionIconType::kPaymentsOfferNotification:
    case PageActionIconType::kContextualSidePanel:
    case PageActionIconType::kJsOptimizations:
    case PageActionIconType::kIndigo:
    case PageActionIconType::kMultistepFilter:
    case PageActionIconType::kRecordReplay:
    case PageActionIconType::kPriceInsights:
    case PageActionIconType::kDiscounts:
    case PageActionIconType::kFederation:
    case PageActionIconType::kCookieControls:
    case PageActionIconType::kManagePasswords:
    case PageActionIconType::kZoom:
    case PageActionIconType::kWebAuthnAmbientSignin:
    case PageActionIconType::kFileSystemAccess:
    case PageActionIconType::kBookmarkStar:
    case PageActionIconType::kAiMode:
    case PageActionIconType::kSaveIban:
    case PageActionIconType::kSaveCard:
    case PageActionIconType::kReadingMode:
    case PageActionIconType::kAutofillPayment:
    case PageActionIconType::kMandatoryReauth:
    case PageActionIconType::kPaymentsChurnedUsers:
    case PageActionIconType::kLensOverlayHomework:
    case PageActionIconType::kFakePageActionForDebug:
    case PageActionIconType::kFilledCardInformation:
    case PageActionIconType::kVirtualCardEnroll:
    case PageActionIconType::kIntentPicker:
    case PageActionIconType::kOptimizationGuide:
      return true;
    default:
      break;
  }

  return false;
}
