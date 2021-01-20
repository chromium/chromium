// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_PASSWORDS_PASSWORD_GENERATION_EDITING_POPUP_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_PASSWORDS_PASSWORD_GENERATION_EDITING_POPUP_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/passwords/password_generation_popup_view.h"
#include "ui/android/view_android.h"

class PasswordGenerationPopupController;

// The android implementation of the password generation explanation popup.
// Note that as opposed to the desktop generation popup, this one is not used
// for displaying a password suggestion, but only for displaying help text
// while the user is editing a generated password.
class PasswordGenerationEditingPopupViewAndroid
    : public PasswordGenerationPopupView {
 public:
  // Builds the UI for the |controller|.
  explicit PasswordGenerationEditingPopupViewAndroid(
      PasswordGenerationPopupController* controller);

  // Called from JNI when the popup was dismissed.
  void Dismissed(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

 private:
  // The popup owns itself.
  virtual ~PasswordGenerationEditingPopupViewAndroid();

  // PasswordGenerationPopupView implementation.
  void Show() override;
  void Hide() override;
  void UpdateState() override;
  void UpdateBoundsAndRedrawPopup() override;
  void PasswordSelectionUpdated() override;

  // Weak pointer to the controller.
  PasswordGenerationPopupController* controller_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // Popup view to be anchored to the container.
  ui::ViewAndroid::ScopedAnchorView popup_;

  DISALLOW_COPY_AND_ASSIGN(PasswordGenerationEditingPopupViewAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_PASSWORDS_PASSWORD_GENERATION_EDITING_POPUP_VIEW_ANDROID_H_
