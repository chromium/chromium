// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JAVASCRIPT_DIALOGS_ANDROID_TAB_MODAL_DIALOG_VIEW_ANDROID_H_
#define COMPONENTS_JAVASCRIPT_DIALOGS_ANDROID_TAB_MODAL_DIALOG_VIEW_ANDROID_H_

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/javascript_dialogs/tab_modal_dialog_view.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace javascript_dialogs {

// An Android version of a JavaScript dialog that automatically dismisses itself
// when the user switches away to a different tab, used for WebContentses that
// are browser tabs.
class TabModalDialogViewAndroid : public TabModalDialogView {
 public:
  TabModalDialogViewAndroid(const TabModalDialogViewAndroid&) = delete;
  TabModalDialogViewAndroid& operator=(const TabModalDialogViewAndroid&) =
      delete;

  ~TabModalDialogViewAndroid() override;

  static base::WeakPtr<TabModalDialogViewAndroid> Create(
      content::WebContents* parent_web_contents,
      content::WebContents* alerting_web_contents,
      const std::u16string& title,
      content::JavaScriptDialogType dialog_type,
      const std::u16string& message_text,
      const std::u16string& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback
          callback_on_button_clicked,
      base::OnceClosure callback_on_cancelled);

  // TabModalDialogView:
  void CloseDialogWithoutCallback() override;
  std::u16string GetUserInput() override;

  void Accept(JNIEnv* env,
              const base::android::JavaParamRef<jobject>&,
              const base::android::JavaParamRef<jstring>& prompt);
  void Cancel(JNIEnv* env,
              const base::android::JavaParamRef<jobject>&,
              jboolean button_clicked);

 private:
  TabModalDialogViewAndroid(
      content::WebContents* parent_web_contents,
      content::WebContents* alerting_web_contents,
      const std::u16string& title,
      content::JavaScriptDialogType dialog_type,
      const std::u16string& message_text,
      const std::u16string& default_prompt_text,
      content::JavaScriptDialogManager::DialogClosedCallback
          callback_on_button_clicked,
      base::OnceClosure callback_on_cancelled);

  std::unique_ptr<TabModalDialogViewAndroid> dialog_;
  base::android::ScopedJavaGlobalRef<jobject> dialog_jobject_;
  JavaObjectWeakGlobalRef jwindow_weak_ref_;

  content::JavaScriptDialogManager::DialogClosedCallback
      callback_on_button_clicked_;
  base::OnceClosure callback_on_cancelled_;

  base::WeakPtrFactory<TabModalDialogViewAndroid> weak_factory_{this};
};

}  // namespace javascript_dialogs

#endif  // COMPONENTS_JAVASCRIPT_DIALOGS_ANDROID_TAB_MODAL_DIALOG_VIEW_ANDROID_H_
