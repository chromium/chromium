// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_NAME_FIX_FLOW_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_NAME_FIX_FLOW_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/ui/payments/card_name_fix_flow_view.h"

namespace content {
class WebContents;
}

namespace autofill {

class CardNameFixFlowController;

class CardNameFixFlowViewAndroid : public CardNameFixFlowView {
 public:
  // |controller| must outlive |this|.
  CardNameFixFlowViewAndroid(CardNameFixFlowController* controller,
                             content::WebContents* web_contents);

  void OnUserAccept(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jstring>& name);
  void PromptDismissed(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj);

  // CardNameFixFlowView implementation.
  void Show() override;
  void ControllerGone() override;

 private:
  ~CardNameFixFlowViewAndroid() override;

  // Returns either the fully initialized java counterpart of this bridge or
  // a is_null() reference if the creation failed. By using this method, the
  // bridge will try to recreate the java object if it failed previously (e.g.
  // because there was no native window available).
  base::android::ScopedJavaGlobalRef<jobject> GetOrCreateJavaObject();

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  base::android::ScopedJavaGlobalRef<jobject> java_object_internal_;

  CardNameFixFlowController* controller_;
  content::WebContents* web_contents_;

  DISALLOW_COPY_AND_ASSIGN(CardNameFixFlowViewAndroid);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_CARD_NAME_FIX_FLOW_VIEW_ANDROID_H_
