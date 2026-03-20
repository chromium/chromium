// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/dialog/autofill_dialog_view_android.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check_deref.h"
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
  // TODO: crbug.com/476753598 - Implement.
}

void AutofillDialogViewAndroid::Dismiss() {
  // TODO: crbug.com/476753598 - Implement.
}

void AutofillDialogViewAndroid::OnPositiveButtonClicked(JNIEnv* env) {
  controller_->OnPositiveButtonClicked();
}

void AutofillDialogViewAndroid::OnDismissed(JNIEnv* env) {
  controller_->OnDismissed();
}

AutofillDialogViewAndroid::~AutofillDialogViewAndroid() = default;

}  // namespace autofill
