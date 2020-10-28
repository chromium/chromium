// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/android/payment_request_test_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/no_destructor.h"
#include "chrome/test/test_support_jni_headers/PaymentRequestTestBridge_jni.h"
#include "content/public/browser/web_contents.h"

namespace payments {

void SetUseDelegateOnPaymentRequestForTesting(
    bool use_delegate,
    bool is_incognito,
    bool is_valid_ssl,
    bool prefs_can_make_payment,
    bool skip_ui_for_basic_card,
    const std::string& twa_package_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_PaymentRequestTestBridge_setUseDelegateForTest(
      env, use_delegate, is_incognito, is_valid_ssl, prefs_can_make_payment,
      skip_ui_for_basic_card,
      base::android::ConvertUTF8ToJavaString(env, twa_package_name));
}

content::WebContents* GetPaymentHandlerWebContentsForTest() {
  JNIEnv* env = base::android::AttachCurrentThread();
  auto jweb_contents =
      Java_PaymentRequestTestBridge_getPaymentHandlerWebContentsForTest(env);
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  return web_contents;
}

bool ClickPaymentHandlerSecurityIconForTest() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PaymentRequestTestBridge_clickPaymentHandlerSecurityIconForTest(
      env);
}

bool ClickPaymentHandlerCloseButtonForTest() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_PaymentRequestTestBridge_clickPaymentHandlerCloseButtonForTest(
      env);
}

bool ConfirmMinimalUIForTest() {
  return Java_PaymentRequestTestBridge_confirmMinimalUIForTest(
      base::android::AttachCurrentThread());
}

bool DismissMinimalUIForTest() {
  return Java_PaymentRequestTestBridge_dismissMinimalUIForTest(
      base::android::AttachCurrentThread());
}

bool IsAndroidMarshmallowOrLollipopForTest() {
  return Java_PaymentRequestTestBridge_isAndroidMarshmallowOrLollipopForTest(
      base::android::AttachCurrentThread());
}

struct NativeObserverCallbacks {
  base::RepeatingClosure on_can_make_payment_called;
  base::RepeatingClosure on_can_make_payment_returned;
  base::RepeatingClosure on_has_enrolled_instrument_called;
  base::RepeatingClosure on_has_enrolled_instrument_returned;
  base::RepeatingClosure on_show_instruments_ready;
  SetAppDescriptionsCallback set_app_descriptions;
  base::RepeatingClosure on_not_supported_error;
  base::RepeatingClosure on_connection_terminated;
  base::RepeatingClosure on_abort_called;
  base::RepeatingClosure on_complete_called;
  base::RepeatingClosure on_minimal_ui_ready;
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
    SetAppDescriptionsCallback set_app_descriptions,
    base::RepeatingClosure on_not_supported_error,
    base::RepeatingClosure on_connection_terminated,
    base::RepeatingClosure on_abort_called,
    base::RepeatingClosure on_complete_called,
    base::RepeatingClosure on_minimal_ui_ready) {
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
  callbacks.set_app_descriptions = std::move(set_app_descriptions);
  callbacks.on_not_supported_error = std::move(on_not_supported_error);
  callbacks.on_connection_terminated = std::move(on_connection_terminated);
  callbacks.on_abort_called = std::move(on_abort_called);
  callbacks.on_complete_called = std::move(on_complete_called);
  callbacks.on_minimal_ui_ready = std::move(on_minimal_ui_ready);

  Java_PaymentRequestTestBridge_setUseNativeObserverForTest(
      env, reinterpret_cast<jlong>(&callbacks.on_can_make_payment_called),
      reinterpret_cast<jlong>(&callbacks.on_can_make_payment_returned),
      reinterpret_cast<jlong>(&callbacks.on_has_enrolled_instrument_called),
      reinterpret_cast<jlong>(&callbacks.on_has_enrolled_instrument_returned),
      reinterpret_cast<jlong>(&callbacks.on_show_instruments_ready),
      reinterpret_cast<jlong>(&callbacks.set_app_descriptions),
      reinterpret_cast<jlong>(&callbacks.on_not_supported_error),
      reinterpret_cast<jlong>(&callbacks.on_connection_terminated),
      reinterpret_cast<jlong>(&callbacks.on_abort_called),
      reinterpret_cast<jlong>(&callbacks.on_complete_called),
      reinterpret_cast<jlong>(&callbacks.on_minimal_ui_ready));
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

static void JNI_PaymentRequestTestBridge_SetAppDescriptions(
    JNIEnv* env,
    jlong callback_ptr,
    const base::android::JavaParamRef<jobjectArray>& japp_labels,
    const base::android::JavaParamRef<jobjectArray>& japp_sublabels,
    const base::android::JavaParamRef<jobjectArray>& japp_totals) {
  std::vector<std::string> app_labels;
  base::android::AppendJavaStringArrayToStringVector(env, japp_labels,
                                                     &app_labels);

  std::vector<std::string> app_sublabels;
  base::android::AppendJavaStringArrayToStringVector(env, japp_sublabels,
                                                     &app_sublabels);
  DCHECK_EQ(app_labels.size(), app_sublabels.size());

  std::vector<std::string> app_totals;
  base::android::AppendJavaStringArrayToStringVector(env, japp_totals,
                                                     &app_totals);
  DCHECK_EQ(app_labels.size(), app_totals.size());

  std::vector<AppDescription> descriptions(app_labels.size());
  for (size_t i = 0; i < app_labels.size(); ++i) {
    descriptions[i].label = app_labels[i];
    descriptions[i].sublabel = app_sublabels[i];
    descriptions[i].total = app_totals[i];
  }

  auto* callback = reinterpret_cast<SetAppDescriptionsCallback*>(callback_ptr);
  callback->Run(descriptions);
}

}  // namespace payments
