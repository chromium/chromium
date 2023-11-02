// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_COLLECT_USER_DATA_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_COLLECT_USER_DATA_DELEGATE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

namespace autofill_assistant {
class UiControllerAndroid;
// Delegate class for CollectUserDataAction, to react on clicks on its chips.
class AssistantCollectUserDataDelegate {
 public:
  explicit AssistantCollectUserDataDelegate(UiControllerAndroid* ui_controller);
  ~AssistantCollectUserDataDelegate();

  void OnContactInfoChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jobject>& jcontact_profile,
      jint event_type);

  void OnPhoneNumberChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jobject>& jphone_number,
      jint event_type);

  void OnShippingAddressChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jobject>& jaddress,
      jint event_type);

  void OnCreditCardChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jobject>& jcard,
      const base::android::JavaParamRef<jobject>& jbilling_profile,
      jint event_type);

  void OnTermsAndConditionsChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jint state);

  void OnTextLinkClicked(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jcaller,
                         jint link);

  void OnLoginChoiceChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& jidentifier,
      jint event_type);

  void OnKeyValueChanged(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& jcaller,
                         const base::android::JavaParamRef<jstring>& jkey,
                         const base::android::JavaParamRef<jobject>& jvalue);

  void OnInputTextFocusChanged(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      jboolean jis_focused);

  base::android::ScopedJavaGlobalRef<jobject> GetJavaObject();

 private:
  raw_ptr<UiControllerAndroid> ui_controller_;

  // Java-side AssistantCollectUserDataDelegate object.
  base::android::ScopedJavaGlobalRef<jobject>
      java_assistant_collect_user_data_delegate_;
};
}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ANDROID_ASSISTANT_COLLECT_USER_DATA_DELEGATE_H_
