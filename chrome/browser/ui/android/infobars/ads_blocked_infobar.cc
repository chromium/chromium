// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/ads_blocked_infobar.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/AdsBlockedInfoBar_jni.h"

using base::android::JavaParamRef;

AdsBlockedInfoBar::AdsBlockedInfoBar(
    std::unique_ptr<AdsBlockedInfobarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

AdsBlockedInfoBar::~AdsBlockedInfoBar() {}

base::android::ScopedJavaLocalRef<jobject>
AdsBlockedInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  using base::android::ConvertUTF16ToJavaString;
  using base::android::ScopedJavaLocalRef;
  AdsBlockedInfobarDelegate* ads_blocked_delegate =
      static_cast<AdsBlockedInfobarDelegate*>(delegate());
  ScopedJavaLocalRef<jstring> reload_button_text = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  ScopedJavaLocalRef<jstring> ok_button_text = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  ScopedJavaLocalRef<jstring> message_text =
      ConvertUTF16ToJavaString(env, ads_blocked_delegate->GetMessageText());
  ScopedJavaLocalRef<jstring> explanation_message =
      ConvertUTF16ToJavaString(env, ads_blocked_delegate->GetExplanationText());

  ScopedJavaLocalRef<jstring> toggle_text =
      ConvertUTF16ToJavaString(env, ads_blocked_delegate->GetToggleText());
  return Java_AdsBlockedInfoBar_show(env, GetEnumeratedIconId(), message_text,
                                     ok_button_text, reload_button_text,
                                     toggle_text, explanation_message);
}
