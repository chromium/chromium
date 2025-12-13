// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/android/android_info.h"

namespace syncer {

std::string GetPersonalizableDeviceNameInternal() {
  return base::android::android_info::model();
}

}  // namespace syncer
