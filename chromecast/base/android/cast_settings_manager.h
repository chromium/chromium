// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_ANDROID_CAST_SETTINGS_MANAGER_H_
#define CHROMECAST_BASE_ANDROID_CAST_SETTINGS_MANAGER_H_

#include <string>

namespace chromecast {

class CastSettingsManager {
 public:
  CastSettingsManager() {}

  static bool UpdateGlobalDeviceName(const std::string& deviceName);
  static bool HasWriteSecureSettingsPermission();
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_ANDROID_CAST_SETTINGS_MANAGER_H_
