// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/messages_feature.h"

#include "base/metrics/field_trial_params.h"

namespace messages {

const base::Feature kMessagesForAndroidAdsBlocked{
    "MessagesForAndroidAdsBlocked", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidChromeSurvey{
    "MessagesForAndroidChromeSurvey", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidInfrastructure{
    "MessagesForAndroidInfrastructure", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidInstantApps{
    "MessagesForAndroidInstantApps", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidNearOomReduction{
    "MessagesForAndroidNearOomReduction", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidNotificationBlocked{
    "MessagesForAndroidNotificationBlocked", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidOfferNotification{
    "MessagesForAndroidOfferNotification", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidPasswords{
    "MessagesForAndroidPasswords", base::FEATURE_ENABLED_BY_DEFAULT};

constexpr base::FeatureParam<int>
    kMessagesForAndroidPasswords_MessageDismissDurationMs{
        &kMessagesForAndroidPasswords,
        "save_password_message_dismiss_duration_ms", 20000};

const base::Feature kMessagesForAndroidPermissionUpdate{
    "MessagesForAndroidPermissionUpdate", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidPopupBlocked{
    "MessagesForAndroidPopupBlocked", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidReaderMode{
    "MessagesForAndroidReaderMode", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidSafetyTip{
    "MessagesForAndroidSafetyTip", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidSaveCard{
    "MessagesForAndroidSaveCard", base::FEATURE_DISABLED_BY_DEFAULT};

constexpr base::FeatureParam<bool>
    kMessagesForAndroidSaveCard_UseFollowupButtonText{
        &kMessagesForAndroidSaveCard,
        "save_card_message_use_followup_button_text", false};

constexpr base::FeatureParam<bool> kMessagesForAndroidSaveCard_UseGPayIcon{
    &kMessagesForAndroidSaveCard, "save_card_message_use_gpay_icon", true};

constexpr base::FeatureParam<bool> kMessagesForAndroidSaveCard_UseDialogV2{
    &kMessagesForAndroidSaveCard, "save_card_dialog_v2_enabled", false};

const base::Feature kMessagesForAndroidStackingAnimation{
    "MessagesForAndroidStackingAnimation", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidSyncError{
    "MessagesForAndroidSyncError", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kMessagesForAndroidUpdatePassword{
    "MessagesForAndroidUpdatePassword", base::FEATURE_ENABLED_BY_DEFAULT};

constexpr base::FeatureParam<bool>
    kMessagesForAndroidUpdatePassword_UseFollowupButtonText{
        &kMessagesForAndroidUpdatePassword, "use_followup_button_text", false};

const base::Feature kMessagesForAndroidReduceLayoutChanges{
    "MessagesForAndroidReduceLayoutChanges", base::FEATURE_ENABLED_BY_DEFAULT};

bool IsAdsBlockedMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidAdsBlocked);
}

bool IsInstantAppsMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidInstantApps);
}

bool IsNearOomReductionMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidNearOomReduction);
}

bool IsOfferNotificationMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidOfferNotification);
}

bool IsPasswordMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidPasswords);
}

bool IsPopupBlockedMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidPopupBlocked);
}

bool IsSafetyTipMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidSafetyTip);
}

bool IsSaveCardMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidSaveCard);
}

bool IsUpdatePasswordMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidUpdatePassword);
}

bool UseFollowupButtonTextForUpdatePasswordButton() {
  return kMessagesForAndroidUpdatePassword_UseFollowupButtonText.Get();
}

bool IsNotificationBlockedMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidNotificationBlocked);
}

bool IsPermissionUpdateMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidPermissionUpdate);
}

int GetSavePasswordMessageDismissDurationMs() {
  return kMessagesForAndroidPasswords_MessageDismissDurationMs.Get();
}

bool UseFollowupButtonTextForSaveCardMessage() {
  return kMessagesForAndroidSaveCard_UseFollowupButtonText.Get();
}

bool UseGPayIconForSaveCardMessage() {
  return kMessagesForAndroidSaveCard_UseGPayIcon.Get();
}

bool UseDialogV2ForSaveCardMessage() {
  return kMessagesForAndroidSaveCard_UseDialogV2.Get();
}

}  // namespace messages
