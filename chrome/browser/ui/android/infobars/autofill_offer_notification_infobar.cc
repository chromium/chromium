// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/infobars/autofill_offer_notification_infobar.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/android/chrome_jni_headers/AutofillOfferNotificationInfoBar_jni.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "components/autofill/core/browser/payments/autofill_offer_notification_infobar_delegate_mobile.h"
#include "components/autofill/core/browser/payments/autofill_save_card_infobar_mobile.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::ScopedJavaLocalRef;

AutofillOfferNotificationInfoBar::AutofillOfferNotificationInfoBar(
    std::unique_ptr<autofill::AutofillOfferNotificationInfoBarDelegateMobile>
        delegate)
    : infobars::ConfirmInfoBar(std::move(delegate)) {}

AutofillOfferNotificationInfoBar::~AutofillOfferNotificationInfoBar() {}

void AutofillOfferNotificationInfoBar::OnOfferDeepLinkClicked(
    JNIEnv* env,
    jobject obj,
    const base::android::JavaParamRef<jobject>& url) {
  GetOfferNotificationDelegate()->OnOfferDeepLinkClicked(
      *url::GURLAndroid::ToNativeGURL(env, url));
}

base::android::ScopedJavaLocalRef<jobject>
AutofillOfferNotificationInfoBar::CreateRenderInfoBar(
    JNIEnv* env,
    const ResourceIdMapper& resource_id_mapper) {
  autofill::AutofillOfferNotificationInfoBarDelegateMobile* delegate =
      GetOfferNotificationDelegate();

  base::android::ScopedJavaLocalRef<jobject> java_delegate =
      Java_AutofillOfferNotificationInfoBar_create(
          env, reinterpret_cast<intptr_t>(this),
          resource_id_mapper.Run(delegate->GetIconId()),
          base::android::ConvertUTF16ToJavaString(env,
                                                  delegate->GetMessageText()),
          base::android::ConvertUTF16ToJavaString(
              env, GetTextFor(ConfirmInfoBarDelegate::BUTTON_OK)),
          url::GURLAndroid::FromNativeGURL(env, delegate->deep_link_url()));

  Java_AutofillOfferNotificationInfoBar_setCreditCardDetails(
      env, java_delegate,
      base::android::ConvertUTF16ToJavaString(
          env, delegate->credit_card_identifier_string()),
      resource_id_mapper.Run(delegate->network_icon_id()));

  return java_delegate;
}

autofill::AutofillOfferNotificationInfoBarDelegateMobile*
AutofillOfferNotificationInfoBar::GetOfferNotificationDelegate() {
  return static_cast<autofill::AutofillOfferNotificationInfoBarDelegateMobile*>(
      GetDelegate());
}
