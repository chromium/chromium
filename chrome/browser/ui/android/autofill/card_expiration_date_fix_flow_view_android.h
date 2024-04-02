// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_EXPIRATION_DATE_FIX_FLOW_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_EXPIRATION_DATE_FIX_FLOW_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/ui/payments/card_expiration_date_fix_flow_view.h"

namespace content {
class WebContents;
}

namespace autofill {

class CardExpirationDateFixFlowController;

class CardExpirationDateFixFlowViewAndroid
    : public CardExpirationDateFixFlowView {
 public:
  CardExpirationDateFixFlowViewAndroid(
      CardExpirationDateFixFlowController* controller,
      content::WebContents* web_contents);

  CardExpirationDateFixFlowViewAndroid(
      const CardExpirationDateFixFlowViewAndroid&) = delete;
  CardExpirationDateFixFlowViewAndroid& operator=(
      const CardExpirationDateFixFlowViewAndroid&) = delete;

  void OnUserAccept(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const std::u16string& month,
                    const std::u16string& year);
  void OnUserDismiss(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);
  void PromptDismissed(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

  // CardExpirationDateFixFlowView implementation.
  void Show() override;
  void ControllerGone() override;

 private:
  ~CardExpirationDateFixFlowViewAndroid() override;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  raw_ptr<CardExpirationDateFixFlowController> controller_;

  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_EXPIRATION_DATE_FIX_FLOW_VIEW_ANDROID_H_
