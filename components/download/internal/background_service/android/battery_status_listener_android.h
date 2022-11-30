// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_ANDROID_BATTERY_STATUS_LISTENER_ANDROID_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_ANDROID_BATTERY_STATUS_LISTENER_ANDROID_H_

#include "components/download/internal/background_service/scheduler/battery_status_listener_impl.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

namespace download {

// Backed by a Java class that holds helper functions to query battery status.
class BatteryStatusListenerAndroid : public BatteryStatusListenerImpl {
 public:
  BatteryStatusListenerAndroid(const base::TimeDelta& battery_query_interval);

  BatteryStatusListenerAndroid(const BatteryStatusListenerAndroid&) = delete;
  BatteryStatusListenerAndroid& operator=(const BatteryStatusListenerAndroid&) =
      delete;

  ~BatteryStatusListenerAndroid() override;

  // BatteryStatusListener implementation.
  int GetBatteryPercentageInternal() override;
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_ANDROID_BATTERY_STATUS_LISTENER_ANDROID_H_
