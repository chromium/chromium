// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/dialog/autofill_dialog_view_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check_deref.h"
#include "chrome/android/chrome_jni_headers/AutofillDialogController_jni.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using base::android::ConvertUTF16ToJavaString;

namespace autofill {

AutofillDialogViewAndroid::AutofillDialogViewAndroid(
    AutofillDialogController* controller)
    : controller_(CHECK_DEREF(controller)) {}

// static
std::unique_ptr<AutofillDialogView> AutofillDialogView::Create(
    AutofillDialogController* controller) {
  return std::make_unique<AutofillDialogViewAndroid>(controller);
}

void AutofillDialogViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = controller_->GetWebContents().GetNativeView();
  if (!view_android) {
    return;
  }
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android) {
    return;
  }

  java_object_.Reset(Java_AutofillDialogController_create(
      env, reinterpret_cast<intptr_t>(this), window_android->GetJavaObject()));
  // Java object might be null if the context associated with the android
  // window doesn't exist.
  if (!java_object_.is_null()) {
    Java_AutofillDialogController_show(
        env, java_object_, controller_->GetTitleText(),
        controller_->GetDescriptionText(), controller_->GetButtonText());
  }
}

void AutofillDialogViewAndroid::Dismiss() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_object_.is_null()) {
    Java_AutofillDialogController_dismiss(env, java_object_);
  }
}

void AutofillDialogViewAndroid::OnPositiveButtonClicked(JNIEnv* env) {
  controller_->OnPositiveButtonClicked();
}

void AutofillDialogViewAndroid::OnDismissed(JNIEnv* env) {
  controller_->OnDismissed();
}

AutofillDialogViewAndroid::~AutofillDialogViewAndroid() = default;

}  // namespace autofill

DEFINE_JNI(AutofillDialogController)
