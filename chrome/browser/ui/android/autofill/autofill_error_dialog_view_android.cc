// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_error_dialog_view_android.h"

#include <jni.h>
#include <stddef.h>
#include "chrome/browser/android/resource_mapper.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/payments/view_factory.h"
#include "components/grit/components_scaled_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/resource/resource_bundle.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/ui/android/autofill/internal/jni_headers/AutofillErrorDialogBridge_jni.h"

using base::android::ConvertUTF16ToJavaString;

namespace autofill {

AutofillErrorDialogViewAndroid::AutofillErrorDialogViewAndroid(
    AutofillErrorDialogController* controller)
    : controller_(controller->GetWeakPtr()) {}

AutofillErrorDialogViewAndroid::~AutofillErrorDialogViewAndroid() = default;

void AutofillErrorDialogViewAndroid::Dismiss() {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!java_object_.is_null()) {
    Java_AutofillErrorDialogBridge_dismiss(env, java_object_);
  } else {
    OnDismissed(env);
  }
}

base::WeakPtr<AutofillErrorDialogView>
AutofillErrorDialogViewAndroid::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void AutofillErrorDialogViewAndroid::OnDismissed(JNIEnv* env) {
  if (controller_) {
    controller_->OnDismissed();
  }
  controller_ = nullptr;
  // Must delete itself when the view is dismissed to avoid memory leak as this
  // class is not owned by other autofill components.
  delete this;
}

void AutofillErrorDialogViewAndroid::Show(content::WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = web_contents->GetNativeView();
  DCHECK(view_android);
  ui::WindowAndroid* window_android = view_android->GetWindowAndroid();
  if (!window_android)
    return;

  java_object_.Reset(Java_AutofillErrorDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this), window_android->GetJavaObject()));

  if (!controller_) {
    return;
  }

  Java_AutofillErrorDialogBridge_show(
      env, java_object_, ConvertUTF16ToJavaString(env, controller_->GetTitle()),
      ConvertUTF16ToJavaString(env, controller_->GetDescription()),
      ConvertUTF16ToJavaString(env, controller_->GetButtonLabel()),
      ResourceMapper::MapToJavaDrawableId(
          IDR_AUTOFILL_GOOGLE_PAY_WITH_DIVIDER));
}

base::WeakPtr<AutofillErrorDialogView> CreateAndShowAutofillErrorDialog(
    AutofillErrorDialogController* controller,
    content::WebContents* web_contents) {
  AutofillErrorDialogViewAndroid* dialog_view =
      new AutofillErrorDialogViewAndroid(controller);
  dialog_view->Show(web_contents);
  return dialog_view->GetWeakPtr();
}

}  // namespace autofill
