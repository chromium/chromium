// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/snackbar/autofill_snackbar_view_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/resource/resource_bundle.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AutofillSnackbarController_jni.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

AutofillSnackbarViewAndroid::AutofillSnackbarViewAndroid(
    AutofillSnackbarController* controller)
    : controller_(controller) {}

AutofillSnackbarView* AutofillSnackbarView::Create(
    AutofillSnackbarController* controller) {
  return new AutofillSnackbarViewAndroid(controller);
}

void AutofillSnackbarViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android =
      controller_->GetWebContents()->GetNativeView();
  DCHECK(view_android);
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android)
    return;

  java_object_.Reset(Java_AutofillSnackbarController_create(
      env, reinterpret_cast<intptr_t>(this), window_android->GetJavaObject()));
  Java_AutofillSnackbarController_show(
      env, java_object_, controller_->GetMessageText(),
      controller_->GetActionButtonText(),
      static_cast<int>(controller_->GetDuration().InMilliseconds()));
}

void AutofillSnackbarViewAndroid::Dismiss() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_object_.is_null()) {
    Java_AutofillSnackbarController_dismiss(env, java_object_);
  } else {
    OnDismissed(env);
  }
}

void AutofillSnackbarViewAndroid::OnActionClicked(JNIEnv* env) {
  controller_->OnActionClicked();
}

void AutofillSnackbarViewAndroid::OnDismissed(JNIEnv* env) {
  controller_->OnDismissed();
  delete this;
}

AutofillSnackbarViewAndroid::~AutofillSnackbarViewAndroid() = default;

}  // namespace autofill
