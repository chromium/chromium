// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/android/payment_request_test_bridge.h"

#include "base/no_destructor.h"
#include "chrome/test/test_support_jni_headers/PaymentRequestTestBridge_jni.h"

namespace payments {

void SetUseDelegateOnPaymentRequestForTesting(bool use_delegate,
                                              bool is_incognito,
                                              bool is_valid_ssl,
                                              bool is_web_contents_active,
                                              bool prefs_can_make_payment,
                                              bool skip_ui_for_basic_card) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PaymentRequestTestBridge_setUseDelegateForTest(
      env, use_delegate, is_incognito, is_valid_ssl, is_web_contents_active,
      prefs_can_make_payment, skip_ui_for_basic_card);
}

struct NativeObserverCallbacks {
  base::RepeatingClosure on_can_make_payment_called;
  base::RepeatingClosure on_can_make_payment_returned;
  base::RepeatingClosure on_has_enrolled_instrument_called;
  base::RepeatingClosure on_has_enrolled_instrument_returned;
  base::RepeatingClosure on_show_instruments_ready;
  base::RepeatingClosure on_not_supported_error;
  base::RepeatingClosure on_connection_terminated;
  base::RepeatingClosure on_abort_called;
};

static NativeObserverCallbacks& GetNativeObserverCallbacks() {
  static base::NoDestructor<NativeObserverCallbacks> callbacks;
  return *callbacks;
}

void SetUseNativeObserverOnPaymentRequestForTesting(
    base::RepeatingClosure on_can_make_payment_called,
    base::RepeatingClosure on_can_make_payment_returned,
    base::RepeatingClosure on_has_enrolled_instrument_called,
    base::RepeatingClosure on_has_enrolled_instrument_returned,
    base::RepeatingClosure on_show_instruments_ready,
    base::RepeatingClosure on_not_supported_error,
    base::RepeatingClosure on_connection_terminated,
    base::RepeatingClosure on_abort_called) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // Store ownership of the callbacks so that we can pass a pointer to Java.
  NativeObserverCallbacks& callbacks = GetNativeObserverCallbacks();
  callbacks.on_can_make_payment_called = std::move(on_can_make_payment_called);
  callbacks.on_can_make_payment_returned =
      std::move(on_can_make_payment_returned);
  callbacks.on_has_enrolled_instrument_called =
      std::move(on_has_enrolled_instrument_called);
  callbacks.on_has_enrolled_instrument_returned =
      std::move(on_has_enrolled_instrument_returned);
  callbacks.on_show_instruments_ready = std::move(on_show_instruments_ready);
  callbacks.on_not_supported_error = std::move(on_not_supported_error);
  callbacks.on_connection_terminated = std::move(on_connection_terminated);
  callbacks.on_abort_called = std::move(on_abort_called);

  Java_PaymentRequestTestBridge_setUseNativeObserverForTest(
      env, reinterpret_cast<jlong>(&callbacks.on_can_make_payment_called),
      reinterpret_cast<jlong>(&callbacks.on_can_make_payment_returned),
      reinterpret_cast<jlong>(&callbacks.on_has_enrolled_instrument_called),
      reinterpret_cast<jlong>(&callbacks.on_has_enrolled_instrument_returned),
      reinterpret_cast<jlong>(&callbacks.on_show_instruments_ready),
      reinterpret_cast<jlong>(&callbacks.on_not_supported_error),
      reinterpret_cast<jlong>(&callbacks.on_connection_terminated),
      reinterpret_cast<jlong>(&callbacks.on_abort_called));
}

// This runs callbacks given to SetUseNativeObserverOnPaymentRequestForTesting()
// by casting them back from a long. The pointer is kept valid once it is passed
// to Java so that this cast and use is valid. They are RepeatingClosures that
// we expect to be called multiple times, so the callback object is not
// destroyed after running it.
static void JNI_PaymentRequestTestBridge_ResolvePaymentRequestObserverCallback(
    JNIEnv* env,
    jlong callback_ptr) {
  auto* callback = reinterpret_cast<base::RepeatingClosure*>(callback_ptr);
  callback->Run();
}

}  // namespace payments
