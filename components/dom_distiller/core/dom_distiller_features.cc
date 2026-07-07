// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_features.h"

#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "components/dom_distiller/core/dom_distiller_switches.h"
#include "components/dom_distiller/core/pref_names.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/feature_map.h"
#include "base/no_destructor.h"
#include "components/dom_distiller/core/android/jni_headers/DomDistillerFeatureMap_jni.h"
#endif

namespace dom_distiller {

bool IsDomDistillerEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDomDistiller);
}

bool ShouldStartDistillabilityService() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDistillabilityService);
}


#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kReaderModeSupportNewFonts, base::FEATURE_ENABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID)
// Feature declarations below -- alphabetical order.
BASE_FEATURE(kReaderModeBlurTransitionAnimation,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReaderModeDelayBottomSheetPeek,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReaderModeDistillInApp, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kReaderModeToggleLinks, base::FEATURE_DISABLED_BY_DEFAULT);

namespace android {
static int64_t JNI_DomDistillerFeatureMap_GetNativeMap(JNIEnv* env) {
  static const base::Feature* const kFeaturesExposedToJava[] = {
      &kReaderModeDelayBottomSheetPeek, &kReaderModeDistillInApp,
      &kReaderModeSupportNewFonts, &kReaderModeToggleLinks};
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return reinterpret_cast<int64_t>(kFeatureMap.get());
}
}  // namespace android
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace dom_distiller

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(DomDistillerFeatureMap)
#endif
