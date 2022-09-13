// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/display/de_jelly.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "components/viz/common/common_jni_headers/DeJellyUtils_jni.h"
#endif

namespace viz {

bool DeJellyEnabled() {
  static bool enabled =
      !base::FeatureList::IsEnabled(features::kDisableDeJelly) &&
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableDeJelly);
  return enabled;
}

bool DeJellyActive() {
  if (!DeJellyEnabled())
    return false;

#if BUILDFLAG(IS_ANDROID)
  return Java_DeJellyUtils_useDeJelly(base::android::AttachCurrentThread());
#else
  return true;
#endif
}

float DeJellyScreenWidth() {
  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDeJellyScreenWidth);
  if (!value.empty())
    return std::atoi(value.c_str());

#if BUILDFLAG(IS_ANDROID)
  return Java_DeJellyUtils_screenWidth(base::android::AttachCurrentThread());
#else
  return 1440.0f;
#endif
}

float MaxDeJellyHeight() {
  // Not currently configurable.
  return 30.0f;
}

}  // namespace viz
