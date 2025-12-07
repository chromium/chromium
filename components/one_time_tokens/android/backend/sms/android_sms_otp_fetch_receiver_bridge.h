// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_RECEIVER_BRIDGE_H_
#define COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_RECEIVER_BRIDGE_H_

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/one_time_tokens/android/backend/sms/android_sms_otp_fetch_receiver_bridge_interface.h"
#include "components/one_time_tokens/android/backend/sms/sms_otp_retrieval_api_error_codes.h"

namespace one_time_tokens {

// A bridge to communicate Java OTP fetcher replies back to the native code.
// Lives on the UI thread.
class AndroidSmsOtpFetchReceiverBridge
    : public one_time_tokens::AndroidSmsOtpFetchReceiverBridgeInterface {
 public:
  // Factory function for creating the bridge.
  static std::unique_ptr<AndroidSmsOtpFetchReceiverBridgeInterface> Create();

  ~AndroidSmsOtpFetchReceiverBridge() override;

  // Returns reference to the Java JNI bridge object.
  base::android::ScopedJavaGlobalRef<jobject> GetJavaBridge() const override;

  // Sets the consumer to be notified when an OTP fetching request finishes.
  void SetConsumer(base::WeakPtr<Consumer> consumer) override;

  // Methods called via JNI from Java.
  void OnOtpValueRetrieved(JNIEnv* env,
                           const base::android::JavaRef<jstring>& otp_value);

  // Implements consumer interface. Called via JNI when OTP value retrieval
  // fails.
  void OnOtpValueRetrievalError(JNIEnv* env, jint api_error_code);

 private:
  AndroidSmsOtpFetchReceiverBridge();

  // Method to be run on the correct sequence when value retrieval succeeds.
  void OnOtpValueRetrievedInternal(const std::string& otp_value);

  // Method to be run on the correct sequence when value retrieval fails.
  void OnOtpValueRetrievalErrorInternal(jint api_error_code);

  // The consumer to be notified when an OTP retrieval request finishes.
  base::WeakPtr<Consumer> consumer_;

  // The Java counterpart to this class.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // To check that all calls are executed on the default UI sequence.
  SEQUENCE_CHECKER(main_sequence_checker_);
  base::WeakPtrFactory<AndroidSmsOtpFetchReceiverBridge> weak_factory_{this};
};

}  // namespace one_time_tokens

#endif  // COMPONENTS_ONE_TIME_TOKENS_ANDROID_BACKEND_SMS_ANDROID_SMS_OTP_FETCH_RECEIVER_BRIDGE_H_
