// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_DISPATCHER_BRIDGE_H_
#define COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_DISPATCHER_BRIDGE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "components/one_time_tokens/android/backend/sms/android_sms_otp_fetch_dispatcher_bridge_interface.h"

namespace one_time_tokens {

// A bridge to send OTP fetch requests to Java. Lives on the background thread.
class AndroidSmsOtpFetchDispatcherBridge
    : public AndroidSmsOtpFetchDispatcherBridgeInterface {
 public:
  // Factory function for creating the bridge.
  static std::unique_ptr<AndroidSmsOtpFetchDispatcherBridgeInterface> Create();

  ~AndroidSmsOtpFetchDispatcherBridge() override;

  // AndroidSmsOtpFetchDispatcherBridgeInterface:
  bool Init(
      base::android::ScopedJavaGlobalRef<jobject> receiver_bridge) override;

  void RetrieveSmsOtp() override;

 private:
  AndroidSmsOtpFetchDispatcherBridge();

  // The Java counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_
      GUARDED_BY_CONTEXT(thread_checker_);

  // All operations should be called on the same background thread.
  // As sequence does not guarantee execution on the same thread in general,
  // a `SEQUENCE_CHECKER` is not sufficient here.
  THREAD_CHECKER(thread_checker_);
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_DISPATCHER_BRIDGE_H_
