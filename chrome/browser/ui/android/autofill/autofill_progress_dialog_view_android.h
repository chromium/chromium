// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PROGRESS_DIALOG_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PROGRESS_DIALOG_VIEW_ANDROID_H_

#include <jni.h>
#include <stddef.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_controller.h"
#include "chrome/browser/ui/autofill/payments/autofill_progress_dialog_view.h"

namespace autofill {

// Android implementation of the AutofillProgressDialogView. This view is owned
// by the `AutofillProgressDialogControllerImpl` which lives for the duration of
// the tab.
class AutofillProgressDialogViewAndroid : public AutofillProgressDialogView {
 public:
  explicit AutofillProgressDialogViewAndroid(
      AutofillProgressDialogController* controller);
  ~AutofillProgressDialogViewAndroid() override;

  // AutofillProgressDialogView.
  void Dismiss(bool show_confirmation_before_closing,
               bool is_canceled_by_user) override;

  // Called by the Java code when the progress dialog is dismissed.
  void OnDismissed(JNIEnv* env);

  // Show the dialog view.
  void ShowDialog();

  // Show the confirmation icon and text.
  void ShowConfirmation(std::u16string confirmation_message);

 private:
  raw_ptr<AutofillProgressDialogController> controller_;
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PROGRESS_DIALOG_VIEW_ANDROID_H_
