// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/dom_distiller_features.h"

#include <string>

#include "base/command_line.h"
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

#if BUILDFLAG(IS_ANDROID)
// Feature declarations below -- alphabetical order.
BASE_FEATURE(kReaderModeAutoDistill,
             "ReaderModeAutoDistill",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kReaderModeImprovements,
             "ReaderModeImprovements",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace android {
static jlong JNI_DomDistillerFeatureMap_GetNativeMap(JNIEnv* env) {
  static const base::Feature* const kFeaturesExposedToJava[] = {
      &kReaderModeAutoDistill, &kReaderModeImprovements};
  static base::NoDestructor<base::android::FeatureMap> kFeatureMap(
      kFeaturesExposedToJava);
  return reinterpret_cast<jlong>(kFeatureMap.get());
}
}  // namespace android
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace dom_distiller
