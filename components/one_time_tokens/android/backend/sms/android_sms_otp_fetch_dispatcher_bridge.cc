// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/one_time_tokens/android/backend/sms/android_sms_otp_fetch_dispatcher_bridge.h"

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/one_time_tokens/android/backend/sms/jni_headers/AndroidSmsOtpFetchDispatcherBridge_jni.h"

namespace one_time_tokens {

// static
std::unique_ptr<AndroidSmsOtpFetchDispatcherBridgeInterface>
AndroidSmsOtpFetchDispatcherBridge::Create() {
  return base::WrapUnique(new AndroidSmsOtpFetchDispatcherBridge());
}

AndroidSmsOtpFetchDispatcherBridge::AndroidSmsOtpFetchDispatcherBridge() {
  // This is needed because the bridge is constructed from the main thread,
  // but is later used only on the background thread.
  DETACH_FROM_THREAD(thread_checker_);
}

AndroidSmsOtpFetchDispatcherBridge::~AndroidSmsOtpFetchDispatcherBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

bool AndroidSmsOtpFetchDispatcherBridge::Init(
    base::android::ScopedJavaGlobalRef<jobject> receiver_bridge) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  java_object_ = Java_AndroidSmsOtpFetchDispatcherBridge_create(
      base::android::AttachCurrentThread(), receiver_bridge);
  return !java_object_.is_null();
}

void AndroidSmsOtpFetchDispatcherBridge::RetrieveSmsOtp() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  CHECK(!java_object_.is_null());
  Java_AndroidSmsOtpFetchDispatcherBridge_retrieveSmsOtp(
      base::android::AttachCurrentThread(), java_object_);
}

}  // namespace one_time_tokens

DEFINE_JNI(AndroidSmsOtpFetchDispatcherBridge)
