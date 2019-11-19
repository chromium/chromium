// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/confirm_infobar.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/ConfirmInfoBar_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

// InfoBarService -------------------------------------------------------------

std::unique_ptr<infobars::InfoBar> InfoBarService::CreateConfirmInfoBar(
    std::unique_ptr<ConfirmInfoBarDelegate> delegate) {
  return std::make_unique<ConfirmInfoBar>(std::move(delegate));
}


// ConfirmInfoBar -------------------------------------------------------------

ConfirmInfoBar::ConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate> delegate)
    : InfoBarAndroid(std::move(delegate)) {}

ConfirmInfoBar::~ConfirmInfoBar() {
}

base::string16 ConfirmInfoBar::GetTextFor(
    ConfirmInfoBarDelegate::InfoBarButton button) {
  ConfirmInfoBarDelegate* delegate = GetDelegate();
  return (delegate->GetButtons() & button) ?
      delegate->GetButtonLabel(button) : base::string16();
}

ConfirmInfoBarDelegate* ConfirmInfoBar::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}

TabAndroid* ConfirmInfoBar::GetTab() {
  content::WebContents* web_contents =
      InfoBarService::WebContentsFromInfoBar(this);
  DCHECK(web_contents);

  TabAndroid* tab = TabAndroid::FromWebContents(web_contents);
  DCHECK(tab);
  return tab;
}

ScopedJavaLocalRef<jobject> ConfirmInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  ScopedJavaLocalRef<jstring> ok_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK));
  ScopedJavaLocalRef<jstring> cancel_button_text =
      base::android::ConvertUTF16ToJavaString(
          env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL));
  ConfirmInfoBarDelegate* delegate = GetDelegate();
  ScopedJavaLocalRef<jstring> message_text =
      base::android::ConvertUTF16ToJavaString(env, delegate->GetMessageText());
  ScopedJavaLocalRef<jstring> link_text =
      base::android::ConvertUTF16ToJavaString(env, delegate->GetLinkText());

  ScopedJavaLocalRef<jobject> java_bitmap;
  if (delegate->GetIconId() == infobars::InfoBarDelegate::kNoIconID &&
      !delegate->GetIcon().IsEmpty()) {
    java_bitmap = gfx::ConvertToJavaBitmap(delegate->GetIcon().ToSkBitmap());
  }

  return Java_ConfirmInfoBar_create(env, GetEnumeratedIconId(), java_bitmap,
                                    message_text, link_text, ok_button_text,
                                    cancel_button_text);
}

void ConfirmInfoBar::OnLinkClicked(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  if (!owner())
      return; // We're closing; don't call anything, it might access the owner.

  if (GetDelegate()->LinkClicked(WindowOpenDisposition::NEW_FOREGROUND_TAB))
    RemoveSelf();
}

void ConfirmInfoBar::ProcessButton(int action) {
  if (!owner())
    return; // We're closing; don't call anything, it might access the owner.

  DCHECK((action == InfoBarAndroid::ACTION_OK) ||
      (action == InfoBarAndroid::ACTION_CANCEL));
  ConfirmInfoBarDelegate* delegate = GetDelegate();
  if ((action == InfoBarAndroid::ACTION_OK) ?
      delegate->Accept() : delegate->Cancel())
    RemoveSelf();
}
