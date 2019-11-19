// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/snackbars/auto_signin_prompt_controller.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/AutoSigninSnackbarController_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::ScopedJavaLocalRef;

void ShowAutoSigninPrompt(content::WebContents* web_contents,
                          const base::string16& username) {
  base::string16 message = l10n_util::GetStringFUTF16(
      IDS_MANAGE_PASSWORDS_AUTO_SIGNIN_TITLE, username);

  JNIEnv* env = base::android::AttachCurrentThread();
  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  // TODO(melandory): https://crbug.com/590838 Introduce proper fix.
  if (tab == nullptr)
    return;
  ScopedJavaLocalRef<jstring> java_message =
      base::android::ConvertUTF16ToJavaString(env, message);
  Java_AutoSigninSnackbarController_showSnackbar(env, tab->GetJavaObject(),
                                                 java_message);
}
