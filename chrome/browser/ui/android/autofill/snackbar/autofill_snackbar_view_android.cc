// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/snackbar/autofill_snackbar_view_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

// Must come after headers that provide symbols used by @JniType.
#include "chrome/android/chrome_jni_headers/AutofillSnackbarController_jni.h"

namespace autofill {

AutofillSnackbarViewAndroid::AutofillSnackbarViewAndroid(
    AutofillSnackbarController* controller)
    : controller_(controller) {}

AutofillSnackbarView* AutofillSnackbarView::Create(
    AutofillSnackbarController* controller) {
  return new AutofillSnackbarViewAndroid(controller);
}

void AutofillSnackbarViewAndroid::Show() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  ui::ViewAndroid* view_android =
      controller_->GetWebContents()->GetNativeView();
  DCHECK(view_android);
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android) {
    return;
  }

  java_object_.Reset(Java_AutofillSnackbarController_create(
      env, reinterpret_cast<intptr_t>(this), window_android));
  Java_AutofillSnackbarController_show(
      env, java_object_, controller_->GetMessageText(),
      controller_->GetActionButtonText(),
      static_cast<int>(controller_->GetDuration().InMilliseconds()),
      controller_->GetSnackbarType());
}

void AutofillSnackbarViewAndroid::Dismiss() {
  if (!java_object_.is_null()) {
    JNIEnv* env = jni_zero::AttachCurrentThread();
    Java_AutofillSnackbarController_dismiss(env, java_object_);
  }
  delete this;
}

void AutofillSnackbarViewAndroid::OnActionClicked() {
  AutofillSnackbarController* const controller = controller_;
  controller->OnActionClicked();
}

void AutofillSnackbarViewAndroid::OnDismissed() {
  AutofillSnackbarController* const controller = controller_;
  controller->OnDismissed();
}

AutofillSnackbarViewAndroid::~AutofillSnackbarViewAndroid() = default;

}  // namespace autofill

DEFINE_JNI(AutofillSnackbarController)
