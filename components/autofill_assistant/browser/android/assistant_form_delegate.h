// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_FORM_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_FORM_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

namespace autofill_assistant {
class UiControllerAndroid;
// Delegate class for the assistant form.
class AssistantFormDelegate {
 public:
  explicit AssistantFormDelegate(UiControllerAndroid* ui_controller);
  ~AssistantFormDelegate();

  void OnCounterChanged(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& jcaller,
                        jint input_index,
                        jint counter_index,
                        jint value);

  void OnChoiceSelectionChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint input_index,
      jint choice_index,
      jboolean selected);

  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller,
                     jint link);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaObject();

 private:
  raw_ptr<UiControllerAndroid> ui_controller_;

  // Java-side AssistantFormDelegate object.
  base::android::ScopedJavaGlobalRef<jobject> java_assistant_form_delegate_;
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_FORM_DELEGATE_H_
