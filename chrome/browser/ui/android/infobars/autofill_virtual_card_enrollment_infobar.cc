// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/autofill_virtual_card_enrollment_infobar.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/autofill/core/browser/metrics/payments/virtual_card_enrollment_metrics.h"
#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/autofill_virtual_card_enrollment_infobar_mobile.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/AutofillVirtualCardEnrollmentInfoBar_jni.h"

using base::android::ScopedJavaLocalRef;

namespace autofill {

std::unique_ptr<infobars::InfoBar> CreateVirtualCardEnrollmentInfoBarMobile(
    std::unique_ptr<AutofillVirtualCardEnrollmentInfoBarDelegateMobile>
        delegate) {
  return std::make_unique<AutofillVirtualCardEnrollmentInfoBar>(
      std::move(delegate));
}

}  // namespace autofill

AutofillVirtualCardEnrollmentInfoBar::AutofillVirtualCardEnrollmentInfoBar(
    std::unique_ptr<
        autofill::AutofillVirtualCardEnrollmentInfoBarDelegateMobile> delegate)
    : infobars::ConfirmInfoBar(std::move(delegate)) {
  virtual_card_enrollment_delegate_ = static_cast<
      autofill::AutofillVirtualCardEnrollmentInfoBarDelegateMobile*>(
      GetDelegate());
}

AutofillVirtualCardEnrollmentInfoBar::~AutofillVirtualCardEnrollmentInfoBar() =
    default;

void AutofillVirtualCardEnrollmentInfoBar::OnInfobarLinkClicked(
    JNIEnv* env,
    jobject obj,
    jstring url,
    jint link_type) {
  virtual_card_enrollment_delegate_->OnInfobarLinkClicked(
      GURL(base::android::ConvertJavaStringToUTF16(env, url)),
      static_cast<autofill::VirtualCardEnrollmentLinkType>(link_type));
}

base::android::ScopedJavaLocalRef<jobject>
AutofillVirtualCardEnrollmentInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  base::android::ScopedJavaLocalRef<jobject> java_delegate =
      Java_AutofillVirtualCardEnrollmentInfoBar_create(
          env, reinterpret_cast<intptr_t>(this),
          resource_id_mapper.Run(
              virtual_card_enrollment_delegate_->GetIconId()),
          ScopedJavaLocalRef<jobject>(),
          base::android::ConvertUTF16ToJavaString(
              env, virtual_card_enrollment_delegate_->GetMessageText()),
          base::android::ConvertUTF16ToJavaString(
              env, virtual_card_enrollment_delegate_->GetLinkText()),
          base::android::ConvertUTF16ToJavaString(
              env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK)),
          base::android::ConvertUTF16ToJavaString(
              env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_CANCEL)));

  Java_AutofillVirtualCardEnrollmentInfoBar_setDescription(
      env, java_delegate,
      base::android::ConvertUTF16ToJavaString(
          env, virtual_card_enrollment_delegate_->GetDescriptionText()),
      base::android::ConvertUTF16ToJavaString(
          env, virtual_card_enrollment_delegate_->GetLearnMoreLinkText()));

  Java_AutofillVirtualCardEnrollmentInfoBar_addCardDetail(
      env, java_delegate,
      gfx::ConvertToJavaBitmap(
          *virtual_card_enrollment_delegate_->GetIssuerIcon()->bitmap()),
      base::android::ConvertUTF16ToJavaString(
          env, virtual_card_enrollment_delegate_->GetCardLabel()));

  for (const auto& line :
       virtual_card_enrollment_delegate_->GetGoogleLegalMessage()) {
    Java_AutofillVirtualCardEnrollmentInfoBar_addGoogleLegalMessageLine(
        env, java_delegate,
        base::android::ConvertUTF16ToJavaString(env, line.text()));
    for (const auto& link : line.links()) {
      Java_AutofillVirtualCardEnrollmentInfoBar_addLinkToLastGoogleLegalMessageLine(
          env, java_delegate, link.range.start(), link.range.end(),
          base::android::ConvertUTF8ToJavaString(env, link.url.spec()));
    }
  }

  if (!virtual_card_enrollment_delegate_->GetIssuerLegalMessage().empty()) {
    for (const auto& line :
         virtual_card_enrollment_delegate_->GetIssuerLegalMessage()) {
      Java_AutofillVirtualCardEnrollmentInfoBar_addIssuerLegalMessageLine(
          env, java_delegate,
          base::android::ConvertUTF16ToJavaString(env, line.text()));
      for (const auto& link : line.links()) {
        Java_AutofillVirtualCardEnrollmentInfoBar_addLinkToLastIssuerLegalMessageLine(
            env, java_delegate, link.range.start(), link.range.end(),
            base::android::ConvertUTF8ToJavaString(env, link.url.spec()));
      }
    }
  }

  return java_delegate;
}
