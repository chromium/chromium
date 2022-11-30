// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_ANDROID_APP_MODAL_DIALOG_VIEW_ANDROID_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_ANDROID_APP_MODAL_DIALOG_VIEW_ANDROID_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "ui/gfx/native_widget_types.h"

namespace javascript_dialogs {

class AppModalDialogController;

class AppModalDialogViewAndroid : public AppModalDialogView {
 public:
  AppModalDialogViewAndroid(JNIEnv* env,
                            AppModalDialogController* controller,
                            gfx::NativeWindow parent);

  AppModalDialogViewAndroid(const AppModalDialogViewAndroid&) = delete;
  AppModalDialogViewAndroid& operator=(const AppModalDialogViewAndroid&) =
      delete;

  ~AppModalDialogViewAndroid() override;

  // AppModalDialogView:
  void ShowAppModalDialog() override;
  void ActivateAppModalDialog() override;
  void CloseAppModalDialog() override;
  void AcceptAppModalDialog() override;
  void CancelAppModalDialog() override;
  bool IsShowing() const override;

  // Called when java confirms or cancels the dialog.
  void DidAcceptAppModalDialog(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& prompt_text,
      bool suppress_js_dialogs);
  void DidCancelAppModalDialog(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>&,
                               bool suppress_js_dialogs);

  const base::android::ScopedJavaGlobalRef<jobject>& GetDialogObject() const;

 private:
  std::unique_ptr<AppModalDialogController> controller_;
  base::android::ScopedJavaGlobalRef<jobject> dialog_jobject_;
  JavaObjectWeakGlobalRef parent_jobject_weak_ref_;
};

}  // namespace javascript_dialogs

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_ANDROID_APP_MODAL_DIALOG_VIEW_ANDROID_H_
