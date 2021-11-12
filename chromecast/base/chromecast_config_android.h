// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_CHROMECAST_CONFIG_ANDROID_H_
#define CHROMECAST_BASE_CHROMECAST_CONFIG_ANDROID_H_

#include "base/callback.h"

namespace chromecast {
namespace android {

class ChromecastConfigAndroid {
 public:
  static ChromecastConfigAndroid* GetInstance();

  ChromecastConfigAndroid(const ChromecastConfigAndroid&) = delete;
  ChromecastConfigAndroid& operator=(const ChromecastConfigAndroid&) = delete;

  // Returns whether or not the user has allowed sending usage stats and
  // crash reports.
  // TODO(ziyangch): Remove CanSendUsageStats() and switch to pure callback
  // style.
  virtual bool CanSendUsageStats() = 0;

  // Set the the user's sending usage stats.
  // TODO(ziyangch): Remove SetSendUsageStats() after switching to Crashpad on
  // Android.(The CL which does this is at https://crrev.com/c/989401.)
  virtual void SetSendUsageStats(bool enabled) = 0;

  // Registers a handler to be notified when SendUsageStats is changed.
  virtual void SetSendUsageStatsChangedCallback(
      base::RepeatingCallback<void(bool)> callback) = 0;

  virtual void RunSendUsageStatsChangedCallback(bool enabled) = 0;

 protected:
  ChromecastConfigAndroid() {}

  virtual ~ChromecastConfigAndroid() {}
};

}  // namespace android
}  // namespace chromecast

#endif  // CHROMECAST_BASE_CHROMECAST_CONFIG_ANDROID_H_
