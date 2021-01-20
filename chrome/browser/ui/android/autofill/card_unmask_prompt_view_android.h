// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_view.h"

namespace content {
class WebContents;
}

namespace autofill {

class CardUnmaskPromptController;

class CardUnmaskPromptViewAndroid : public CardUnmaskPromptView {
 public:
  explicit CardUnmaskPromptViewAndroid(CardUnmaskPromptController* controller,
                                       content::WebContents* web_contents);

  bool CheckUserInputValidity(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& response);
  void OnUserInput(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jstring>& cvc,
                   const base::android::JavaParamRef<jstring>& month,
                   const base::android::JavaParamRef<jstring>& year,
                   jboolean should_store_locally,
                   jboolean enable_fido_auth);
  void OnNewCardLinkClicked(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);
  int GetExpectedCvcLength(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj);
  void PromptDismissed(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

  // CardUnmaskPromptView implementation.
  void Show() override;
  void ControllerGone() override;
  void DisableAndWaitForVerification() override;
  void GotVerificationResult(const base::string16& error_message,
                             bool allow_retry) override;

 private:
  ~CardUnmaskPromptViewAndroid() override;

  // Returns either the fully initialized java counterpart of this bridge or
  // a is_null() reference if the creation failed. By using this method, the
  // bridge will try to recreate the java object if it failed previously (e.g.
  // because there was no native window available).
  base::android::ScopedJavaGlobalRef<jobject> GetOrCreateJavaObject();

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_internal_;

  CardUnmaskPromptController* controller_;
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(CardUnmaskPromptViewAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_UNMASK_PROMPT_VIEW_ANDROID_H_
