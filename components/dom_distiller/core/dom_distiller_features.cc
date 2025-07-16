// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_features.h"

#include <string>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
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

BASE_FEATURE(kReaderModeUseReadability,
             "ReaderModeUseReadability",
             base::FEATURE_DISABLED_BY_DEFAULT);

#if !BUILDFLAG(IS_IOS)
constexpr base::FeatureParam<bool> kReaderModeUseReadabilityUseDistiller{
    &kReaderModeUseReadability, /*name=*/"use_distiller",
    /*default_value=*/false};
#endif

bool ShouldUseReadabilityDistiller() {
#if BUILDFLAG(IS_IOS)
  return base::FeatureList::IsEnabled(kReaderModeUseReadability);
#else
  return base::FeatureList::IsEnabled(kReaderModeUseReadability) &&
         kReaderModeUseReadabilityUseDistiller.Get();
#endif
}

#if BUILDFLAG(IS_ANDROID)
// Feature declarations below -- alphabetical order.
BASE_FEATURE(kReaderModeDistillInApp,
             "ReaderModeDistillInApp",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReaderModeImprovements,
             "ReaderModeImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace android {
static jlong JNI_DomDistillerFeatureMap_GetNativeMap(JNIEnv* env) {
  static const base::Feature* const kFeaturesExposedToJava[] = {
      &kReaderModeDistillInApp, &kReaderModeImprovements,
      &kReaderModeUseReadability};
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return reinterpret_cast<jlong>(kFeatureMap.get());
}
}  // namespace android
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace dom_distiller
