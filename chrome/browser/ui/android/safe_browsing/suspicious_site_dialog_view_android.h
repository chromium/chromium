// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_SAFE_BROWSING_SUSPICIOUS_SITE_DIALOG_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_SAFE_BROWSING_SUSPICIOUS_SITE_DIALOG_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/safe_browsing/android/suspicious_site_controller_android.h"

namespace ui {
class WindowAndroid;
}

namespace safe_browsing {

// Modal dialog to display suspicious site warning. Directly connected with
// SafeBrowsingSuspiciousSiteDialogBridge on Java side.
// Owned by `SuspiciousSiteControllerAndroid`.
class SuspiciousSiteDialogViewAndroid {
 public:
  explicit SuspiciousSiteDialogViewAndroid(
      SuspiciousSiteControllerAndroid& controller);

  SuspiciousSiteDialogViewAndroid(const SuspiciousSiteDialogViewAndroid&) =
      delete;
  SuspiciousSiteDialogViewAndroid& operator=(
      const SuspiciousSiteDialogViewAndroid&) = delete;

  // Destructor must delete its Java counterpart.
  ~SuspiciousSiteDialogViewAndroid();

  // Called from native to Java.
  void Show(ui::WindowAndroid& window_android);

  // Called from Java to native.
  void ContinueAnyway(JNIEnv* env);
  void GoBack(JNIEnv* env);
  void OnLearnMoreClicked(JNIEnv* env);
  void Close(JNIEnv* env,
             ui::ModalDialogWrapper::DismissalCause dismissalCause);

 private:
  // The controller which owns this dialog and handles the dialog events.
  // `controller_` owns `this`.
  const raw_ref<SuspiciousSiteControllerAndroid> controller_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_UI_ANDROID_SAFE_BROWSING_SUSPICIOUS_SITE_DIALOG_VIEW_ANDROID_H_
