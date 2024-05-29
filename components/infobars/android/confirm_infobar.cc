// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/infobars/android/confirm_infobar.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/infobars/android/jni_headers/ConfirmInfoBar_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace infobars {

ConfirmInfoBar::ConfirmInfoBar(std::unique_ptr<ConfirmInfoBarDelegate> delegate)
    : InfoBarAndroid(std::move(delegate)) {}

ConfirmInfoBar::~ConfirmInfoBar() = default;

std::u16string ConfirmInfoBar::GetTextFor(
    ConfirmInfoBarDelegate::InfoBarButton button) {
  ConfirmInfoBarDelegate* delegate = GetDelegate();
  return (delegate->GetButtons() & button) ? delegate->GetButtonLabel(button)
                                           : std::u16string();
}

ConfirmInfoBarDelegate* ConfirmInfoBar::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}

ScopedJavaLocalRef<jobject> ConfirmInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
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
    java_bitmap = gfx::ConvertToJavaBitmap(
        *delegate->GetIcon().Rasterize(nullptr).bitmap());
  }

  return Java_ConfirmInfoBar_create(
      env, resource_id_mapper.Run(delegate->GetIconId()), java_bitmap,
      message_text, link_text, ok_button_text, cancel_button_text);
}

void ConfirmInfoBar::OnLinkClicked(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  if (GetDelegate()->LinkClicked(WindowOpenDisposition::NEW_FOREGROUND_TAB))
    RemoveSelf();
}

void ConfirmInfoBar::ProcessButton(int action) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.

  DCHECK((action == InfoBarAndroid::ACTION_OK) ||
         (action == InfoBarAndroid::ACTION_CANCEL));
  ConfirmInfoBarDelegate* delegate = GetDelegate();
  if ((action == InfoBarAndroid::ACTION_OK) ? delegate->Accept()
                                            : delegate->Cancel())
    RemoveSelf();
}

}  // namespace infobars
