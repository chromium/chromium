// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/previews_lite_page_infobar.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/PreviewsLitePageInfoBar_jni.h"

PreviewsLitePageInfoBar::PreviewsLitePageInfoBar(
    std::unique_ptr<PreviewsLitePageInfoBarDelegate> delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

PreviewsLitePageInfoBar::~PreviewsLitePageInfoBar() {}

// static
std::unique_ptr<infobars::InfoBar> PreviewsLitePageInfoBar::CreateInfoBar(
    infobars::InfoBarManager* infobar_manager,
    std::unique_ptr<PreviewsLitePageInfoBarDelegate> delegate) {
  return std::make_unique<PreviewsLitePageInfoBar>(std::move(delegate));
}

base::android::ScopedJavaLocalRef<jobject>
PreviewsLitePageInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  ConfirmInfoBarDelegate* delegate = GetDelegate();
  base::android::ScopedJavaLocalRef<jstring> message_text =
      base::android::ConvertUTF16ToJavaString(env, delegate->GetMessageText());
  base::android::ScopedJavaLocalRef<jstring> link_text =
      base::android::ConvertUTF16ToJavaString(env, delegate->GetLinkText());
  return Java_PreviewsLitePageInfoBar_show(env, GetEnumeratedIconId(),
                                           message_text, link_text);
}
