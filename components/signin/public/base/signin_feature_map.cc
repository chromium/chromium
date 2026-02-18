// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/feature_map.h"
#include "base/android/jni_android.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/no_destructor.h"
#include "components/signin/public/base/signin_switches.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/signin/public/android/jni_headers/SigninFeatureMap_jni.h"

namespace signin {

namespace {
// Array of features exposed through the Java SigninFeatures API.
const base::Feature* const kFeaturesExposedToJava[] = {
    &switches::kCctSignInPrompt,
    &switches::kEnableActivitylessSigninAllEntryPoint,
    &switches::kEnableAddSessionRedirect,
    &switches::kEnableSeamlessSignin,
    &switches::kForceStartupSigninPromo,
    &switches::kForceHistoryOptInScreen,
    &switches::kSkipCheckForAccountManagementOnSignin,
    &switches::kSyncEnableBookmarksInTransportMode,
    &switches::kMakeIdentityManagerSourceOfAccounts,
    &switches::kMigrateAccountManagerDelegate,
    &switches::kFullscreenSignInPromoUseDate,
    &switches::kSmartEmailLineBreaking,
    &switches::kSupportWebSigninAddSession,
    &switches::kSkipRefreshTokenCheckInIdentityManager,
    &switches::kFRESignInAlternativeSecondaryButtonText,
    &switches::kChromeAndroidIdentitySurveyFirstRun,
    &switches::kChromeAndroidIdentitySurveyWeb,
    &switches::kChromeAndroidIdentitySurveyNtpSigninButton,
    &switches::kChromeAndroidIdentitySurveyNtpAccountAvatarTap,
    &switches::kChromeAndroidIdentitySurveyNtpPromo,
    &switches::kChromeAndroidIdentitySurveyBookmarkPromo,
    &switches::kSigninLevelUpButton,
    &switches::kSigninManagerSeedingFix,
    &switches::kSupportForcedSigninPolicy,
};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return kFeatureMap.get();
}

}  // namespace

static int64_t JNI_SigninFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<int64_t>(GetFeatureMap());
}

}  // namespace signin

DEFINE_JNI(SigninFeatureMap)
