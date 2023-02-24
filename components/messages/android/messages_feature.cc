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

BASE_FEATURE(kMessagesForAndroidInfrastructure,
             "MessagesForAndroidInfrastructure",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidOfferNotification,
             "MessagesForAndroidOfferNotification",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidPermissionUpdate,
             "MessagesForAndroidPermissionUpdate",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidPopupBlocked,
             "MessagesForAndroidPopupBlocked",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidSaveCard,
             "MessagesForAndroidSaveCard",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kMessagesForAndroidStackingAnimation,
             "MessagesForAndroidStackingAnimation",
             base::FEATURE_DISABLED_BY_DEFAULT);

bool IsAdsBlockedMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidAdsBlocked);
}

bool IsOfferNotificationMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidOfferNotification);
}

bool IsPopupBlockedMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidPopupBlocked);
}

bool IsSaveCardMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidSaveCard);
}

bool IsPermissionUpdateMessagesUiEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidPermissionUpdate);
}

bool IsStackingAnimationEnabled() {
  return base::FeatureList::IsEnabled(kMessagesForAndroidInfrastructure) &&
         base::FeatureList::IsEnabled(kMessagesForAndroidStackingAnimation);
}

static jboolean JNI_MessageFeatureList_IsEnabled(
    JNIEnv* env,
    const JavaParamRef<jstring>& jfeature_name) {
  const base::Feature* feature =
      FindFeatureExposedToJava(ConvertJavaStringToUTF8(env, jfeature_name));
  return base::FeatureList::IsEnabled(*feature);
}

}  // namespace messages
