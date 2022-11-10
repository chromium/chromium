// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/messages/android/messages_feature.h"

#include "base/android/jni_string.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "components/messages/android/jni_headers/MessageFeatureList_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;

namespace messages {

namespace {

const base::Feature* kFeaturesExposedToJava[] = {
    &kMessagesForAndroidStackingAnimation,
};

const base::Feature* FindFeatureExposedToJava(const std::string& feature_name) {
  for (const base::Feature* feature : kFeaturesExposedToJava) {
    if (feature->name == feature_name)
      return feature;
  }
  NOTREACHED() << "Queried feature not found in MessageFeatureList: "
               << feature_name;
  return nullptr;
}

}  // namespace

BASE_FEATURE(kMessagesForAndroidAdsBlocked,
             "MessagesForAndroidAdsBlocked",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidChromeSurvey,
             "MessagesForAndroidChromeSurvey",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidInfrastructure,
             "MessagesForAndroidInfrastructure",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidNearOomReduction,
             "MessagesForAndroidNearOomReduction",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidNotificationBlocked,
             "MessagesForAndroidNotificationBlocked",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidOfferNotification,
             "MessagesForAndroidOfferNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidPasswords,
             "MessagesForAndroidPasswords",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<int>
    kMessagesForAndroidPasswords_MessageDismissDurationMs{
        &kMessagesForAndroidPasswords,
        "save_password_message_dismiss_duration_ms", 20000};

BASE_FEATURE(kMessagesForAndroidPermissionUpdate,
             "MessagesForAndroidPermissionUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidPopupBlocked,
             "MessagesForAndroidPopupBlocked",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidReaderMode,
             "MessagesForAndroidReaderMode",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidSaveCard,
             "MessagesForAndroidSaveCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidStackingAnimation,
             "MessagesForAndroidStackingAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidUpdatePassword,
             "MessagesForAndroidUpdatePassword",
             base::FEATURE_ENABLED_BY_DEFAULT);

constexpr base::FeatureParam<bool>
    kMessagesForAndroidUpdatePassword_UseFollowupButtonText{
        &kMessagesForAndroidUpdatePassword, "use_followup_button_text", false};

bool IsAdsBlockedMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidAdsBlocked);
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

static jboolean JNI_MessageFeatureList_IsEnabled(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

}  // namespace messages
