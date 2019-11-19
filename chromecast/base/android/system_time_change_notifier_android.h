// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_ANDROID_SYSTEM_TIME_CHANGE_NOTIFIER_ANDROID_H_
#define CHROMECAST_BASE_ANDROID_SYSTEM_TIME_CHANGE_NOTIFIER_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chromecast/base/system_time_change_notifier.h"

namespace chromecast {

class SystemTimeChangeNotifierAndroid : public SystemTimeChangeNotifier {
 public:
  SystemTimeChangeNotifierAndroid();
  ~SystemTimeChangeNotifierAndroid() override;

  // Called from Java.
  void OnTimeChanged(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jobj);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_notifier_;

  DISALLOW_COPY_AND_ASSIGN(SystemTimeChangeNotifierAndroid);
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_ANDROID_SYSTEM_TIME_CHANGE_NOTIFIER_ANDROID_H_
