// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/autofill_credit_card_filling_infobar.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/AutofillCreditCardFillingInfoBar_jni.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "components/autofill/core/browser/payments/autofill_credit_card_filling_infobar_delegate_mobile.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

using base::android::ScopedJavaLocalRef;

AutofillCreditCardFillingInfoBar::AutofillCreditCardFillingInfoBar(
    std::unique_ptr<autofill::AutofillCreditCardFillingInfoBarDelegateMobile>
        delegate)
    : ConfirmInfoBar(std::move(delegate)) {}

AutofillCreditCardFillingInfoBar::~AutofillCreditCardFillingInfoBar() {}

base::android::ScopedJavaLocalRef<jobject>
AutofillCreditCardFillingInfoBar::CreateRenderInfoBar(JNIEnv* env) {
  autofill::AutofillCreditCardFillingInfoBarDelegateMobile* delegate =
      static_cast<autofill::AutofillCreditCardFillingInfoBarDelegateMobile*>(
          GetDelegate());
  ScopedJavaLocalRef<jobject> java_bitmap;
  if (delegate->GetIconId() == infobars::InfoBarDelegate::kNoIconID &&
      !delegate->GetIcon().IsEmpty()) {
    java_bitmap = gfx::ConvertToJavaBitmap(delegate->GetIcon().ToSkBitmap());
  }

  base::android::ScopedJavaLocalRef<jobject> java_delegate =
      Java_AutofillCreditCardFillingInfoBar_create(
          env, reinterpret_cast<intptr_t>(this), GetEnumeratedIconId(),
          java_bitmap, base::android::ConvertUTF16ToJavaString(
                           env, delegate->GetMessageText()),
          base::android::ConvertUTF16ToJavaString(
              env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK)),
          base::android::ConvertUTF16ToJavaString(
              env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL)));

  Java_AutofillCreditCardFillingInfoBar_addDetail(
      env, java_delegate,
      ResourceMapper::MapFromChromiumId(delegate->issuer_icon_id()),
      base::android::ConvertUTF16ToJavaString(env, delegate->card_label()),
      base::android::ConvertUTF16ToJavaString(env, delegate->card_sub_label()));

  return java_delegate;
}
