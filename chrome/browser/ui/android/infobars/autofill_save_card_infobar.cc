// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/autofill_save_card_infobar.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/autofill/core/browser/payments/autofill_save_card_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/autofill_save_card_infobar_mobile.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AutofillSaveCardInfoBar_jni.h"

using base::android::ScopedJavaLocalRef;

namespace autofill {

std::unique_ptr<infobars::InfoBar> CreateSaveCardInfoBarMobile(
    std::unique_ptr<AutofillSaveCardInfoBarDelegateMobile> delegate) {
  return std::make_unique<AutofillSaveCardInfoBar>(std::move(delegate));
}

}  // namespace autofill

AutofillSaveCardInfoBar::AutofillSaveCardInfoBar(
    std::unique_ptr<autofill::AutofillSaveCardInfoBarDelegateMobile> delegate)
    : infobars::ConfirmInfoBar(std::move(delegate)) {}

AutofillSaveCardInfoBar::~AutofillSaveCardInfoBar() = default;

void AutofillSaveCardInfoBar::OnLegalMessageLinkClicked(JNIEnv* env,
                                                        jobject obj,
                                                        jstring url) {
  GetSaveCardDelegate()->OnLegalMessageLinkClicked(
      GURL(base::android::ConvertJavaStringToUTF16(env, url)));
}

base::android::ScopedJavaLocalRef<jobject>
AutofillSaveCardInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  autofill::AutofillSaveCardInfoBarDelegateMobile* delegate =
      GetSaveCardDelegate();

  base::android::ScopedJavaLocalRef<jobject> java_delegate =
      Java_AutofillSaveCardInfoBar_create(
          env, reinterpret_cast<intptr_t>(this),
          resource_id_mapper.Run(delegate->GetIconId()),
          ScopedJavaLocalRef<jobject>(),
          base::android::ConvertUTF16ToJavaString(env,
                                                  delegate->GetMessageText()),
          base::android::ConvertUTF16ToJavaString(env, delegate->GetLinkText()),
          base::android::ConvertUTF16ToJavaString(
              env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK)),
          base::android::ConvertUTF16ToJavaString(
              env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL)),
          delegate->IsGooglePayBrandingEnabled(),
          delegate->displayed_target_account_email().empty()
              ? nullptr
              : base::android::ConvertUTF16ToJavaString(
                    env, delegate->displayed_target_account_email()),
          delegate->displayed_target_account_avatar().IsEmpty()
              ? nullptr
              : gfx::ConvertToJavaBitmap(
                    *delegate->displayed_target_account_avatar()
                         .AsImageSkia()
                         .bitmap()));

  Java_AutofillSaveCardInfoBar_setDescriptionText(
      env, java_delegate,
      base::android::ConvertUTF16ToJavaString(env,
                                              delegate->GetDescriptionText()));

  Java_AutofillSaveCardInfoBar_addDetail(
      env, java_delegate,
      ResourceMapper::MapToJavaDrawableId(delegate->issuer_icon_id()),
      base::android::ConvertUTF16ToJavaString(env, delegate->card_label()),
      base::android::ConvertUTF16ToJavaString(env, delegate->card_sub_label()));

  for (const auto& line : delegate->legal_message_lines()) {
    Java_AutofillSaveCardInfoBar_addLegalMessageLine(
        env, java_delegate,
        base::android::ConvertUTF16ToJavaString(env, line.text()));
    for (const auto& link : line.links()) {
      Java_AutofillSaveCardInfoBar_addLinkToLastLegalMessageLine(
          env, java_delegate, link.range.start(), link.range.end(),
          base::android::ConvertUTF8ToJavaString(env, link.url.spec()));
    }
  }

  return java_delegate;
}

autofill::AutofillSaveCardInfoBarDelegateMobile*
AutofillSaveCardInfoBar::GetSaveCardDelegate() {
  return static_cast<autofill::AutofillSaveCardInfoBarDelegateMobile*>(
      GetDelegate());
}
