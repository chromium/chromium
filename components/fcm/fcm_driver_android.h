// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FCM_FCM_DRIVER_ANDROID_H_
#define COMPONENTS_FCM_FCM_DRIVER_ANDROID_H_

#include <jni.h>

#include <string>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/sequence_checker.h"
#include "components/fcm/fcm_driver.h"

namespace fcm {

// FCM driver implementation for Android, connecting to FcmBridge via JNI.
class FcmDriverAndroid : public FcmDriver {
 public:
  FcmDriverAndroid();
  ~FcmDriverAndroid() override;

  FcmDriverAndroid(const FcmDriverAndroid&) = delete;
  FcmDriverAndroid& operator=(const FcmDriverAndroid&) = delete;

  // Methods called from Java via JNI:
  void OnInstallationIdRefreshed(JNIEnv* env,
                                 const std::string& installation_id);
  void OnMessageReceived(JNIEnv* env,
                         const std::string& message_id,
                         const std::vector<std::string>& data_keys_and_values,
                         const std::vector<uint8_t>& raw_data);
  void OnMessagesDeleted(JNIEnv* env);

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace fcm

#endif  // COMPONENTS_FCM_FCM_DRIVER_ANDROID_H_
