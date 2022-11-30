// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/android/assistant_collect_user_data_delegate.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "base/android/locale_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill_assistant/android/jni_headers/AssistantCollectUserDataNativeDelegate_jni.h"
#include "components/autofill_assistant/browser/android/ui_controller_android.h"
#include "components/autofill_assistant/browser/android/ui_controller_android_utils.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace autofill_assistant {

AssistantCollectUserDataDelegate::AssistantCollectUserDataDelegate(
    UiControllerAndroid* ui_controller)
    : ui_controller_(ui_controller) {
  java_assistant_collect_user_data_delegate_ =
      Java_AssistantCollectUserDataNativeDelegate_create(
          AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AssistantCollectUserDataDelegate::~AssistantCollectUserDataDelegate() {
  Java_AssistantCollectUserDataNativeDelegate_clearNativePtr(
      AttachCurrentThread(), java_assistant_collect_user_data_delegate_);
}

void AssistantCollectUserDataDelegate::OnContactInfoChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jcontact_profile,
    jint event_type) {
  std::unique_ptr<autofill::AutofillProfile> contact_profile;
  if (jcontact_profile) {
    contact_profile = std::make_unique<autofill::AutofillProfile>();
    ui_controller_android_utils::PopulateAutofillProfileFromJava(
        jcontact_profile, env, contact_profile.get(),
        base::android::GetDefaultLocaleString());
  }

  ui_controller_->OnContactInfoChanged(
      std::move(contact_profile), static_cast<UserDataEventType>(event_type));
}

void AssistantCollectUserDataDelegate::OnPhoneNumberChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jphone_number,
    jint event_type) {
  std::unique_ptr<autofill::AutofillProfile> phone_number_profile;
  if (jphone_number) {
    phone_number_profile = std::make_unique<autofill::AutofillProfile>();
    ui_controller_android_utils::PopulateAutofillProfileFromJava(
        jphone_number, env, phone_number_profile.get(),
        base::android::GetDefaultLocaleString());
  }

  ui_controller_->OnPhoneNumberChanged(
      std::move(phone_number_profile),
      static_cast<UserDataEventType>(event_type));
}

void AssistantCollectUserDataDelegate::OnShippingAddressChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jaddress,
    jint event_type) {
  std::unique_ptr<autofill::AutofillProfile> shipping_address_profile;
  if (jaddress) {
    shipping_address_profile = std::make_unique<autofill::AutofillProfile>();
    ui_controller_android_utils::PopulateAutofillProfileFromJava(
        jaddress, env, shipping_address_profile.get(),
        base::android::GetDefaultLocaleString());
  }

  ui_controller_->OnShippingAddressChanged(
      std::move(shipping_address_profile),
      static_cast<UserDataEventType>(event_type));
}

void AssistantCollectUserDataDelegate::OnCreditCardChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jcard,
    const base::android::JavaParamRef<jobject>& jbilling_profile,
    jint event_type) {
  std::unique_ptr<autofill::CreditCard> credit_card;
  if (jcard) {
    credit_card = std::make_unique<autofill::CreditCard>();
    ui_controller_android_utils::PopulateAutofillCreditCardFromJava(
        jcard, env, credit_card.get(), base::android::GetDefaultLocaleString());
  }

  std::unique_ptr<autofill::AutofillProfile> billing_profile;
  if (jbilling_profile) {
    billing_profile = std::make_unique<autofill::AutofillProfile>();
    ui_controller_android_utils::PopulateAutofillProfileFromJava(
        jbilling_profile, env, billing_profile.get(),
        base::android::GetDefaultLocaleString());
  }

  ui_controller_->OnCreditCardChanged(
      std::move(credit_card), std::move(billing_profile),
      static_cast<UserDataEventType>(event_type));
}

void AssistantCollectUserDataDelegate::OnTermsAndConditionsChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint state) {
  ui_controller_->OnTermsAndConditionsChanged(
      static_cast<TermsAndConditionsState>(state));
}

void AssistantCollectUserDataDelegate::OnTextLinkClicked(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint link) {
  ui_controller_->OnTextLinkClicked(link);
}

void AssistantCollectUserDataDelegate::OnLoginChoiceChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jidentifier,
    jint event_type) {
  std::string identifier =
      ui_controller_android_utils::SafeConvertJavaStringToNative(env,
                                                                 jidentifier);
  ui_controller_->OnLoginChoiceChanged(identifier);
}

void AssistantCollectUserDataDelegate::OnKeyValueChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jkey,
    const base::android::JavaParamRef<jobject>& jvalue) {
  ui_controller_->OnKeyValueChanged(
      ui_controller_android_utils::SafeConvertJavaStringToNative(env, jkey),
      ui_controller_android_utils::ToNativeValue(env, jvalue));
}

void AssistantCollectUserDataDelegate::OnInputTextFocusChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean jis_focused) {
  ui_controller_->OnInputTextFocusChanged(jis_focused);
}

base::android::ScopedJavaGlobalRef<jobject>
AssistantCollectUserDataDelegate::GetJavaObject() {
  return java_assistant_collect_user_data_delegate_;
}

}  // namespace autofill_assistant
