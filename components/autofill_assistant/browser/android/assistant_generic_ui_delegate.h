// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_GENERIC_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_GENERIC_UI_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

namespace autofill_assistant {
class UiControllerAndroid;
// Delegate class for the generic UI. Receives events from the Java UI and
// forwards them to the ui controller.
class AssistantGenericUiDelegate {
 public:
  explicit AssistantGenericUiDelegate(UiControllerAndroid* ui_controller);
  ~AssistantGenericUiDelegate();

  void OnViewClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jview_identifier);

  void OnValueChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jmodel_identifier,
      const base::android::JavaParamRef<jobject>& jvalue);

  void OnTextLinkClicked(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jcaller,
                         jint jlink);

  void OnGenericPopupDismissed(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jpopup_identifier);

  void OnViewContainerCleared(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jview_identifier);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaObject();

 private:
  raw_ptr<UiControllerAndroid> ui_controller_;

  // Java-side AssistantGenericUiDelegate object.
  base::android::ScopedJavaGlobalRef<jobject>
      java_assistant_generic_ui_delegate_;
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_GENERIC_UI_DELEGATE_H_
