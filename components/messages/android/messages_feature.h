// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_
#define COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_

#include "base/feature_list.h"

namespace messages {

// Feature that controls whether "ads blocked" messages use Messages or
// Infobars infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidAdsBlocked);

// Feature that controls whether "survey" prompts use Messages or
// Infobars infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidChromeSurvey);

// Feature that controls whether Messages for Android infrastucture components
// are initialized. When this feature is disabled all individual message
// implementations also fallback to Infobar implementations.
BASE_DECLARE_FEATURE(kMessagesForAndroidInfrastructure);

// Feature that controls whether "instant apps" messages use Messages or
// Infobars infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidInstantApps);

// Feature that controls whether "near OOM reduction" messages use Messages or
// Infobars infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidNearOomReduction);

// Feature that controls whether notifiation blocked prompts use Messages or
// Infobars infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidNotificationBlocked);

// Feature that controls whether offer notifications use Messages or Infobars
// infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidOfferNotification);

// Feature that controls whether "save password" and "saved password
// confirmation" prompts use Messages or Infobars infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidPasswords);

// Feature that controls whether permission update prompts use Messages or
// Infobars infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidPermissionUpdate);

// Feature that controls whether "popup blocked" prompts use Messages or
// Infobars infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidPopupBlocked);

// Feature that controls whether "reader mode" prompts use Messages or
// Infobars infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidReaderMode);

// Feature that controls whether "safety tip" prompts use Messages or
// Infobars infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidSafetyTip);

// Feature that controls whether "save card" prompts use Messages or
// Infobars infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidSaveCard);

// Feature that controls whether Messages for Android should use
// new Stacking Animation.
BASE_DECLARE_FEATURE(kMessagesForAndroidStackingAnimation);

// Feature that controls whether "update password" prompt uses Messages or
// Infobars infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidUpdatePassword);

// Feature that controls whether we always update layout parameters or only
// while the message container is visible.
BASE_DECLARE_FEATURE(kMessagesForAndroidReduceLayoutChanges);

bool IsAdsBlockedMessagesUiEnabled();

bool IsInstantAppsMessagesUiEnabled();

bool IsNearOomReductionMessagesUiEnabled();

bool IsNotificationBlockedMessagesUiEnabled();

bool IsOfferNotificationMessagesUiEnabled();

bool IsPasswordMessagesUiEnabled();

bool IsPermissionUpdateMessagesUiEnabled();

bool IsPopupBlockedMessagesUiEnabled();

bool IsSafetyTipMessagesUiEnabled();

bool IsSaveCardMessagesUiEnabled();

bool IsUpdatePasswordMessagesUiEnabled();

int GetSavePasswordMessageDismissDurationMs();

bool UseFollowupButtonTextForUpdatePasswordButton();

bool UseFollowupButtonTextForSaveCardMessage();

bool UseGPayIconForSaveCardMessage();

bool UseDialogV2ForSaveCardMessage();

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_
