// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/build_info.h"

namespace syncer {

std::string GetPersonalizableDeviceNameInternal() {
  base::android::BuildInfo* android_build_info =
      base::android::BuildInfo::GetInstance();
  return android_build_info->model();
}

}  // namespace syncer
