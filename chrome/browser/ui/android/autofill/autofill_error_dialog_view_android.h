// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_ERROR_DIALOG_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_ERROR_DIALOG_VIEW_ANDROID_H_

#include <jni.h>
#include <stddef.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/ui/payments/autofill_error_dialog_controller.h"
#include "components/autofill/core/browser/ui/payments/autofill_error_dialog_view.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

// Android implementation of the AutofillErrorDialogView. This view is owned by
// the `AutofillErrorDialogControllerImpl` which lives for the duration of
// the tab.
class AutofillErrorDialogViewAndroid : public AutofillErrorDialogView {
 public:
  explicit AutofillErrorDialogViewAndroid(
      AutofillErrorDialogController* controller);
  ~AutofillErrorDialogViewAndroid() override;

  // AutofillErrorDialogView.
  void Dismiss() override;
  base::WeakPtr<AutofillErrorDialogView> GetWeakPtr() override;

  // Called by the Java code when the error dialog is dismissed.
  void OnDismissed(JNIEnv* env);

  // Show the dialog view.
  void Show(content::WebContents* web_contents);

 private:
  base::WeakPtr<AutofillErrorDialogController> controller_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  base::WeakPtrFactory<AutofillErrorDialogViewAndroid> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_ERROR_DIALOG_VIEW_ANDROID_H_
