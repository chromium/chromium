// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_progress_dialog_view_android.h"

#include <jni.h>
#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/autofill/payments/view_factory.h"
#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_controller.h"
#include "components/grit/components_scaled_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/resource/resource_bundle.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/autofill/internal/jni_headers/AutofillProgressDialogBridge_jni.h"

using base::android::ConvertUTF16ToJavaString;

namespace autofill {

AutofillProgressDialogViewAndroid::AutofillProgressDialogViewAndroid(
    base::WeakPtr<AutofillProgressDialogController> controller)
    : controller_(controller) {}

AutofillProgressDialogViewAndroid::~AutofillProgressDialogViewAndroid() =
    default;

void AutofillProgressDialogViewAndroid::Dismiss(
    bool show_confirmation_before_closing,
    bool is_canceled_by_user) {
  if (controller_ && show_confirmation_before_closing) {
    // If the confirmation is shown, the dialog must have been dismissed
    // automatically without user actions.
    DCHECK(!is_canceled_by_user);
    std::u16string confirmation_message = controller_->GetConfirmationMessage();
    controller_->OnDismissed(/*is_canceled_by_user=*/false);
    controller_ = nullptr;
    ShowConfirmation(confirmation_message);
    return;
  }

  if (controller_) {
    controller_->OnDismissed(/*is_canceled_by_user=*/is_canceled_by_user);
    controller_ = nullptr;
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_object_.is_null()) {
    Java_AutofillProgressDialogBridge_dismiss(env, java_object_);
  } else {
    OnDismissed(env);
  }
}

void AutofillProgressDialogViewAndroid::InvalidateControllerForCallbacks() {
  controller_ = nullptr;
}

base::WeakPtr<AutofillProgressDialogView>
AutofillProgressDialogViewAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillProgressDialogViewAndroid::OnDismissed(JNIEnv* env) {
  if (controller_) {
    controller_->OnDismissed(/*is_canceled_by_user=*/true);
    controller_ = nullptr;
  }
  // Must delete itself when the view is dismissed to avoid memory leak as this
  // class is not owned by other autofill components.
  delete this;
}

bool AutofillProgressDialogViewAndroid::ShowDialog(
    content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents->GetNativeView();
  DCHECK(view_android);
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android) {
    return false;
  }

  java_object_.Reset(Java_AutofillProgressDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this), window_android->GetJavaObject()));

  if (controller_) {
    Java_AutofillProgressDialogBridge_showDialog(
        env, java_object_,
        ConvertUTF16ToJavaString(env, controller_->GetLoadingTitle()),
        ConvertUTF16ToJavaString(env, controller_->GetLoadingMessage()),
        ConvertUTF16ToJavaString(env, controller_->GetCancelButtonLabel()),
        ResourceMapper::MapToJavaDrawableId(
            IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER));
    return true;
  }
  return false;
}

void AutofillProgressDialogViewAndroid::ShowConfirmation(
    std::u16string confirmation_message) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_object_.is_null()) {
    Java_AutofillProgressDialogBridge_showConfirmation(
        env, java_object_, ConvertUTF16ToJavaString(env, confirmation_message));
  }
}

base::WeakPtr<AutofillProgressDialogView> CreateAndShowProgressDialog(
    base::WeakPtr<AutofillProgressDialogController> controller,
    content::WebContents* web_contents) {
  AutofillProgressDialogViewAndroid* dialog_view =
      new AutofillProgressDialogViewAndroid(controller);
  if (dialog_view->ShowDialog(web_contents)) {
    return dialog_view->GetWeakPtr();
  }

  delete dialog_view;
  return nullptr;
}

}  // namespace autofill
