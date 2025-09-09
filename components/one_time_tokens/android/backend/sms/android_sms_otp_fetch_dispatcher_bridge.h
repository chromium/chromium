// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_DISPATCHER_BRIDGE_H_
#define COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_DISPATCHER_BRIDGE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/threading/thread_checker.h"
#include "components/one_time_tokens/android/backend/sms/android_sms_otp_fetch_receiver_bridge.h"

// A bridge to send OTP fetch requests to Java. Lives on the background thread.
class AndroidSmsOtpFetchDispatcherBridge {
 public:
  // Factory function for creating the bridge.
  static std::unique_ptr<AndroidSmsOtpFetchDispatcherBridge> Create();

  AndroidSmsOtpFetchDispatcherBridge();
  ~AndroidSmsOtpFetchDispatcherBridge();

  // Perform bridge and Java counterpart initialization.
  // `receiver_bridge` is the java counterpart of the
  // `AndroidSmsOtpFetchReceiverBridge` and should outlive this object.
  // Returns true if initialization is successful and the Java counterpart
  // is created.
  bool Init(base::android::ScopedJavaGlobalRef<jobject> receiver_bridge);

  // Asynchronously requests the OTP value received via SMS from the backend.
  void RetrieveSmsOtp();

 private:
  // The Java counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_
      GUARDED_BY_CONTEXT(thread_checker_);

  // All operations should be called on the same background thread.
  // As sequence does not guarantee execution on the same thread in general,
  // a `SEQUENCE_CHECKER` is not sufficient here.
  THREAD_CHECKER(thread_checker_);
};

#endif  // COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_DISPATCHER_BRIDGE_H_
