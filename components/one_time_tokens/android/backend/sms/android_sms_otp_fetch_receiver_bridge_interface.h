// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_RECEIVER_BRIDGE_INTERFACE_H_
#define COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_RECEIVER_BRIDGE_INTERFACE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/one_time_tokens/android/backend/sms/sms_otp_retrieval_api_error_codes.h"

namespace one_time_tokens {

// Interface for `AndroidSmsOtpFetchReceiverBridge`.
class AndroidSmsOtpFetchReceiverBridgeInterface {
 public:
  // A bridge is created with a consumer that will be called when an OTP
  // fetching request is completed.
  class Consumer {
   public:
    virtual ~Consumer() = default;

    // Asynchronous response called when an OTP value is retrieved.
    virtual void OnOtpValueRetrieved(std::string value) = 0;

    // Asynchronous response called if there was an error while fetching an OTP
    // value.
    virtual void OnOtpValueRetrievalError(
        SmsOtpRetrievalApiErrorCode error_code) = 0;
  };

  virtual ~AndroidSmsOtpFetchReceiverBridgeInterface() = default;

  // Returns reference to the Java JNI bridge object.
  virtual base::android::ScopedJavaGlobalRef<jobject> GetJavaBridge() const = 0;

  // Sets the consumer to be notified when an OTP fetching request finishes.
  virtual void SetConsumer(base::WeakPtr<Consumer> consumer) = 0;
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_RECEIVER_BRIDGE_INTERFACE_H_
