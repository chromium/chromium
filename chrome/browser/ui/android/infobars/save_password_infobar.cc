// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/save_password_infobar.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/SavePasswordInfoBar_jni.h"
#include "chrome/browser/password_manager/android/save_password_infobar_delegate_android.h"
#include "components/password_manager/core/common/credential_manager_types.h"

using base::android::JavaParamRef;

SavePasswordInfoBar::SavePasswordInfoBar(
    std::unique_ptr<SavePasswordInfoBarDelegate> delegate,
    base::Optional<AccountInfo> account_info)
    : ChromeConfirmInfoBar(std::move(delegate)) {
  account_info_ = account_info;
}

SavePasswordInfoBar::~SavePasswordInfoBar() = default;

base::android::ScopedJavaLocalRef<jobject>
SavePasswordInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  using base::android::ConvertUTF16ToJavaString;
  using base::android::ScopedJavaLocalRef;
  SavePasswordInfoBarDelegate* save_password_delegate =
      static_cast<SavePasswordInfoBarDelegate*>(delegate());
  ScopedJavaLocalRef<jstring> ok_button_text = ConvertUTF16ToJavaString(
      env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  ScopedJavaLocalRef<jstring> cancel_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  ScopedJavaLocalRef<jstring> message_text =
      ConvertUTF16ToJavaString(env, save_password_delegate->GetMessageText());
  ScopedJavaLocalRef<jstring> details_message_text = ConvertUTF16ToJavaString(
      env, save_password_delegate->GetDetailsMessageText());
  ScopedJavaLocalRef<jobject> account_info =
      account_info_.has_value()
          ? ConvertToJavaAccountInfo(env, account_info_.value())
          : nullptr;

  base::android::ScopedJavaLocalRef<jobject> infobar;
  infobar.Reset(Java_SavePasswordInfoBar_show(
      env, resource_id_mapper.Run(delegate()->GetIconId()), message_text,
      details_message_text, ok_button_text, cancel_button_text, account_info));

  java_infobar_.Reset(env, infobar.obj());
  return infobar;
}

void SavePasswordInfoBar::OnLinkClicked(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  delegate()->LinkClicked(WindowOpenDisposition::NEW_FOREGROUND_TAB);
}
