// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/input/utils.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#include "components/input/features.h"
#endif

namespace input {

bool TransferInputToViz() {
#if BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(input::features::kInputOnViz) &&
      (base::android::BuildInfo::GetInstance()->sdk_int() >=
       base::android::SdkVersion::SDK_VERSION_V)) {
    return true;
  }
#endif
  return false;
}

}  // namespace input
