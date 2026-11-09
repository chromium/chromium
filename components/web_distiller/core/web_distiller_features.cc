// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_distiller/core/web_distiller_features.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "components/web_distiller/core/pref_names.h"
#include "components/web_distiller/core/web_distiller_switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "components/web_distiller/core/android/jni_headers/WebDistillerFeatureMap_jni.h"
#endif

namespace web_distiller {

bool IsWebDistillerEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableWebDistiller);
}

bool ShouldStartDistillabilityService() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDistillabilityService);
}

BASE_FEATURE(kSyncReaderForAccessibility, base::FEATURE_ENABLED_BY_DEFAULT);

#if BUILDFLAG(IS_ANDROID)
// Feature declarations below -- alphabetical order.
BASE_FEATURE(kReaderModeBlurTransitionAnimation,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReaderModeDelayBottomSheetPeek,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReaderModeToggleLinks, base::FEATURE_DISABLED_BY_DEFAULT);

namespace android {
static int64_t JNI_WebDistillerFeatureMap_GetNativeMap(JNIEnv* env) {
  static const base::Feature* const kFeaturesExposedToJava[] = {
      &kReaderModeDelayBottomSheetPeek, &kReaderModeToggleLinks};
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return reinterpret_cast<int64_t>(kFeatureMap.get());
}
}  // namespace android
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace web_distiller

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(WebDistillerFeatureMap)
#endif
