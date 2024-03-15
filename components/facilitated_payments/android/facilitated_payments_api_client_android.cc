// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/facilitated_payments_api_client_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "components/facilitated_payments/android/java/jni_headers/FacilitatedPaymentsApiClientBridge_jni.h"

namespace payments::facilitated {

FacilitatedPaymentsApiClientAndroid::FacilitatedPaymentsApiClientAndroid()
    : java_bridge_(Java_FacilitatedPaymentsApiClientBridge_Constructor(
          base::android::AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this))) {}

FacilitatedPaymentsApiClientAndroid::~FacilitatedPaymentsApiClientAndroid() {
  Java_FacilitatedPaymentsApiClientBridge_resetNativePointer(
      base::android::AttachCurrentThread(), java_bridge_);
}

void FacilitatedPaymentsApiClientAndroid::IsAvailable() {
  Java_FacilitatedPaymentsApiClientBridge_isAvailable(
      base::android::AttachCurrentThread(), java_bridge_);
}

void FacilitatedPaymentsApiClientAndroid::GetClientToken() {
  Java_FacilitatedPaymentsApiClientBridge_getClientToken(
      base::android::AttachCurrentThread(), java_bridge_);
}

void FacilitatedPaymentsApiClientAndroid::InvokePurchaseAction(
    base::span<const uint8_t> action_token) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_FacilitatedPaymentsApiClientBridge_invokePurchaseAction(
      env, java_bridge_, base::android::ToJavaByteArray(env, action_token));
}

void FacilitatedPaymentsApiClientAndroid::OnIsAvailable(
    JNIEnv* env,
    jboolean is_pix_payment_available) {}

void FacilitatedPaymentsApiClientAndroid::OnGetClientToken(
    JNIEnv* env,
    jobject jclient_token_byte_array) {}

void FacilitatedPaymentsApiClientAndroid::OnPurchaseActionResult(
    JNIEnv* env,
    jboolean is_purchase_action_successful) {}

}  // namespace payments::facilitated
