// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_CARD_MESSAGE_CONFIRM_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_CARD_MESSAGE_CONFIRM_DELEGATE_H_

#include <jni.h>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"

namespace autofill {

// An interface for interaction with SaveCardMessageConfirmController. Will
// be notified of events by controller.
class SaveCardMessageConfirmDelegate {
 public:
  virtual void OnNameConfirmed(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& name) = 0;

  virtual void OnDateConfirmed(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& month,
      const base::android::JavaParamRef<jstring>& year) = 0;

  virtual void OnSaveCardConfirmed(JNIEnv* env) = 0;

  virtual void DialogDismissed(JNIEnv* env) = 0;

  virtual void OnLinkClicked(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& url) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_SAVE_CARD_MESSAGE_CONFIRM_DELEGATE_H_
