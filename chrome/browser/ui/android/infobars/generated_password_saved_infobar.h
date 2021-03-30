// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_INFOBARS_GENERATED_PASSWORD_SAVED_INFOBAR_H_
#define CHROME_BROWSER_UI_ANDROID_INFOBARS_GENERATED_PASSWORD_SAVED_INFOBAR_H_

#include <jni.h>

#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "chrome/browser/password_manager/android/generated_password_saved_infobar_delegate_android.h"
#include "components/infobars/android/infobar_android.h"

// The Android infobar that notifies that the generated password was saved.
class GeneratedPasswordSavedInfoBar : public infobars::InfoBarAndroid {
 public:
  explicit GeneratedPasswordSavedInfoBar(
      std::unique_ptr<GeneratedPasswordSavedInfoBarDelegateAndroid> delegate);
  ~GeneratedPasswordSavedInfoBar() override;

 private:
  // infobars::InfoBarAndroid implementation:
  base::android::ScopedJavaLocalRef<jobject> CreateRenderInfoBar(
      JNIEnv* env,
      const ResourceIdMapper& resource_id_mapper) override;
  void OnLinkClicked(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj) override;
  void ProcessButton(int action) override;

  DISALLOW_COPY_AND_ASSIGN(GeneratedPasswordSavedInfoBar);
};

#endif  // CHROME_BROWSER_UI_ANDROID_INFOBARS_GENERATED_PASSWORD_SAVED_INFOBAR_H_
