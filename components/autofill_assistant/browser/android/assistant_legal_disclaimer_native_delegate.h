// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_LEGAL_DISCLAIMER_NATIVE_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_LEGAL_DISCLAIMER_NATIVE_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

namespace autofill_assistant {
class UiControllerAndroid;
// Delegate class for the Legal Disclaimer Code. Receives events from the Java
// UI and forwards them to the ui controller. This is the JNI bridge to
// |AssistantLegalDisclaimerNativeDelegate.java|.
class AssistantLegalDisclaimerNativeDelegate {
 public:
  explicit AssistantLegalDisclaimerNativeDelegate(
      UiControllerAndroid* ui_controller);
  ~AssistantLegalDisclaimerNativeDelegate();

  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& jcaller,
                     jint link);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaObject();

 private:
  raw_ptr<UiControllerAndroid> ui_controller_ = nullptr;

  // Java-side AssistantLegalDisclaimerNativeDelegate object.
  base::android::ScopedJavaGlobalRef<jobject> java_native_delegate_;
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_LEGAL_DISCLAIMER_NATIVE_DELEGATE_H_
