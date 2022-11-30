// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_SNACKBAR_AUTOFILL_SNACKBAR_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_SNACKBAR_AUTOFILL_SNACKBAR_VIEW_ANDROID_H_

#include <jni.h>
#include <stddef.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/autofill/payments/autofill_snackbar_controller.h"
#include "chrome/browser/ui/autofill/payments/autofill_snackbar_view.h"

namespace autofill {

// Android implementation of the AutofillSnackbarView. This view is owned by the
// |autofill_popup_controller_impl| which lives for the duration of the tab.
// However, this view would be killed by the controller once the tab loses
// focus.
class AutofillSnackbarViewAndroid : public AutofillSnackbarView {
 public:
  explicit AutofillSnackbarViewAndroid(AutofillSnackbarController* controller);
  ~AutofillSnackbarViewAndroid() override;

  // Show the snackbar.
  void Show() override;
  // Dismiss the snackbar.
  void Dismiss() override;

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------
  void OnActionClicked(JNIEnv* env);
  void OnDismissed(JNIEnv* env);

 private:
  raw_ptr<AutofillSnackbarController> controller_;
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};
}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_SNACKBAR_AUTOFILL_SNACKBAR_VIEW_ANDROID_H_
