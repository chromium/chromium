// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/display/de_jelly.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "components/viz/common/features.h"
#include "components/viz/common/switches.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/time/time.h"
#include "components/viz/common/common_jni_headers/DeJellyUtils_jni.h"
#endif

namespace viz {

bool DeJellyEnabled() {
  if (base::FeatureList::IsEnabled(features::kDisableDeJelly))
    return false;

  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDeJelly);
}

bool DeJellyActive() {
  if (!DeJellyEnabled())
    return false;

#if defined(OS_ANDROID)
  return Java_DeJellyUtils_useDeJelly(base::android::AttachCurrentThread());
#endif

  return true;
}

float DeJellyScreenWidth() {
  std::string value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDeJellyScreenWidth);
  if (!value.empty())
    return std::atoi(value.c_str());

#if defined(OS_ANDROID)
  return Java_DeJellyUtils_screenWidth(base::android::AttachCurrentThread());
#endif

  return 1440.0f;
}

float MaxDeJellyHeight() {
  // Not currently configurable.
  return 30.0f;
}

}  // namespace viz
