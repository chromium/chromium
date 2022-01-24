// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/chromecast_config_android.h"

#include "base/no_destructor.h"

namespace chromecast {
namespace android {

// Dummy implementation of android::ChromecastConfigAndroid.
class ChromecastConfigAndroidDummy : public ChromecastConfigAndroid {
 public:
  ChromecastConfigAndroidDummy() {}

  ChromecastConfigAndroidDummy(const ChromecastConfigAndroidDummy&) = delete;
  ChromecastConfigAndroidDummy& operator=(const ChromecastConfigAndroidDummy&) =
      delete;

  ~ChromecastConfigAndroidDummy() override {}

  bool CanSendUsageStats() override { return false; }

  void SetSendUsageStats(bool enabled) override {}

  void SetSendUsageStatsChangedCallback(
      base::RepeatingCallback<void(bool)> callback) override {}

  void RunSendUsageStatsChangedCallback(bool enabled) override {}

 private:
  friend class base::NoDestructor<ChromecastConfigAndroidDummy>;
};

// static
ChromecastConfigAndroid* ChromecastConfigAndroid::GetInstance() {
  static base::NoDestructor<ChromecastConfigAndroidDummy> instance;
  return instance.get();
}

}  // namespace android
}  // namespace chromecast
