// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/android/backend/sms/android_sms_otp_fetch_receiver_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/one_time_tokens/android/backend/sms/jni_headers/AndroidSmsOtpFetchReceiverBridge_jni.h"

namespace one_time_tokens {

// static
std::unique_ptr<AndroidSmsOtpFetchReceiverBridgeInterface>
AndroidSmsOtpFetchReceiverBridge::Create() {
  return base::WrapUnique(new AndroidSmsOtpFetchReceiverBridge());
}

AndroidSmsOtpFetchReceiverBridge::AndroidSmsOtpFetchReceiverBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  java_object_ = Java_AndroidSmsOtpFetchReceiverBridge_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AndroidSmsOtpFetchReceiverBridge::~AndroidSmsOtpFetchReceiverBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  Java_AndroidSmsOtpFetchReceiverBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

base::android::ScopedJavaGlobalRef<jobject>
AndroidSmsOtpFetchReceiverBridge::GetJavaBridge() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  return java_object_;
}

void AndroidSmsOtpFetchReceiverBridge::SetConsumer(
    base::WeakPtr<Consumer> consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
  consumer_ = std::move(consumer);
}

void AndroidSmsOtpFetchReceiverBridge::OnOtpValueRetrieved(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& otp_value) {
  std::string otp_str = base::android::ConvertJavaStringToUTF8(env, otp_value);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AndroidSmsOtpFetchReceiverBridge::OnOtpValueRetrievedInternal,
          weak_factory_.GetWeakPtr(), otp_str));
}

void AndroidSmsOtpFetchReceiverBridge::OnOtpValueRetrievedInternal(
    const std::string& otp_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  if (!consumer_) {
    return;
  }
  consumer_->OnOtpValueRetrieved(otp_value);
}

void AndroidSmsOtpFetchReceiverBridge::OnOtpValueRetrievalError(
    JNIEnv* env,
    jint api_error_code) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &AndroidSmsOtpFetchReceiverBridge::OnOtpValueRetrievalErrorInternal,
          weak_factory_.GetWeakPtr(), api_error_code));
}

void AndroidSmsOtpFetchReceiverBridge::OnOtpValueRetrievalErrorInternal(
    jint api_error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  if (!consumer_) {
    return;
  }
  consumer_->OnOtpValueRetrievalError(
      static_cast<SmsOtpRetrievalApiErrorCode>(api_error_code));
}

}  // namespace one_time_tokens

DEFINE_JNI(AndroidSmsOtpFetchReceiverBridge)
