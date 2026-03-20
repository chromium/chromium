// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_DIALOG_AUTOFILL_DIALOG_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_DIALOG_AUTOFILL_DIALOG_VIEW_ANDROID_H_

#include <jni.h>
#include <stddef.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"

namespace autofill {

// Android implementation of the AutofillDialogView.
class AutofillDialogViewAndroid : public AutofillDialogView {
 public:
  explicit AutofillDialogViewAndroid(AutofillDialogController* controller);
  ~AutofillDialogViewAndroid() override;

  void Show() override;
  void Dismiss() override;

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------
  void OnPositiveButtonClicked(JNIEnv* env);
  void OnDismissed(JNIEnv* env);

 private:
  raw_ref<AutofillDialogController> controller_;
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_DIALOG_AUTOFILL_DIALOG_VIEW_ANDROID_H_
