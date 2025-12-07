// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_DISPATCHER_BRIDGE_INTERFACE_H_
#define COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_DISPATCHER_BRIDGE_INTERFACE_H_

#include "base/android/scoped_java_ref.h"

namespace one_time_tokens {

// Interface for `AndroidSmsOtpFetchDispatcherBridge`.
class AndroidSmsOtpFetchDispatcherBridgeInterface {
 public:
  virtual ~AndroidSmsOtpFetchDispatcherBridgeInterface() = default;

  // Perform bridge and Java counterpart initialization.
  // `receiver_bridge` is the java counterpart of the
  // `AndroidSmsOtpFetchReceiverBridge` and should outlive this object.
  // Returns true if initialization is successful and the Java counterpart
  // is created.
  virtual bool Init(
      base::android::ScopedJavaGlobalRef<jobject> receiver_bridge) = 0;

  // Asynchronously requests the OTP value received via SMS from the backend.
  virtual void RetrieveSmsOtp() = 0;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_DISPATCHER_BRIDGE_INTERFACE_H_
