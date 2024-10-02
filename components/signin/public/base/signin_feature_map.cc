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
    &switches::kSkipCheckForAccountManagementOnSignin,
    &switches::kRestoreSignedInAccountAndSettingsFromBackup,
    &switches::kHideSettingsSignInPromo,
    &switches::kUseConsentLevelSigninForLegacyAccountEmailPref,
    &switches::kDontFallbackToDefaultImplementationInAccountManagerFacade,
    &switches::kCctSignInPrompt};

// static
base::android::FeatureMap* GetFeatureMap() {
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(std::vector(
      std::begin(kFeaturesExposedToJava), std::end(kFeaturesExposedToJava)));
  return kFeatureMap.get();
}

}  // namespace

static jlong JNI_SigninFeatureMap_GetNativeMap(JNIEnv* env) {
  return reinterpret_cast<jlong>(GetFeatureMap());
}

}  // namespace signin
