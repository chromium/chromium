// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PROGRESS_DIALOG_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PROGRESS_DIALOG_VIEW_ANDROID_H_

#include "components/autofill/core/browser/ui/payments/autofill_progress_dialog_view.h"

#include <jni.h>
#include <stddef.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents.h"

namespace autofill {

class AutofillProgressDialogController;

// Android implementation of the AutofillProgressDialogView. This class
// must delete itself when the view is dismissed to avoid memory leak as
// it is not owned by other autofill components.
class AutofillProgressDialogViewAndroid : public AutofillProgressDialogView {
 public:
  explicit AutofillProgressDialogViewAndroid(
      base::WeakPtr<AutofillProgressDialogController> controller);
  ~AutofillProgressDialogViewAndroid() override;

  // AutofillProgressDialogView.
  void Dismiss(bool show_confirmation_before_closing,
               bool is_canceled_by_user) override;
  void InvalidateControllerForCallbacks() override;
  base::WeakPtr<AutofillProgressDialogView> GetWeakPtr() override;

  // Called by the Java code when the progress dialog is dismissed.
  void OnDismissed(JNIEnv* env);

  // Show the dialog view. Return value indicates whether the dialog is
  // successfully shown.
  bool ShowDialog(content::WebContents* web_contents);

  // Show the confirmation icon and text.
  void ShowConfirmation(std::u16string confirmation_message);

 private:
  base::WeakPtr<AutofillProgressDialogController> controller_;
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  base::WeakPtrFactory<AutofillProgressDialogViewAndroid> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_PROGRESS_DIALOG_VIEW_ANDROID_H_
