// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/fast_checkout/ui_view_android_utils.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/android/fast_checkout/jni_headers/FastCheckoutAutofillProfile_jni.h"
#include "chrome/browser/ui/android/fast_checkout/jni_headers/FastCheckoutCreditCard_jni.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "url/android/gurl_android.h"

using ::base::android::ConvertUTF16ToJavaString;
using ::base::android::ConvertUTF8ToJavaString;

base::android::ScopedJavaLocalRef<jobject> CreateFastCheckoutAutofillProfile(
    JNIEnv* env,
    const autofill::AutofillProfile& profile,
    const std::string& locale) {
  const std::string country_code =
      base::UTF16ToUTF8(profile.GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  const autofill::AutofillCountry country(country_code, locale);
  return Java_FastCheckoutAutofillProfile_Constructor(
      env, ConvertUTF8ToJavaString(env, profile.guid()),
      ConvertUTF8ToJavaString(env, profile.origin()),
      profile.record_type() == autofill::AutofillProfile::LOCAL_PROFILE,
      ConvertUTF16ToJavaString(
          env, profile.GetInfo(autofill::NAME_HONORIFIC_PREFIX, locale)),
      ConvertUTF16ToJavaString(env,
                               profile.GetInfo(autofill::NAME_FULL, locale)),
      ConvertUTF16ToJavaString(env, profile.GetRawInfo(autofill::COMPANY_NAME)),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS)),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(autofill::ADDRESS_HOME_STATE)),
      ConvertUTF16ToJavaString(env,
                               profile.GetRawInfo(autofill::ADDRESS_HOME_CITY)),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY)),
      ConvertUTF16ToJavaString(env,
                               profile.GetRawInfo(autofill::ADDRESS_HOME_ZIP)),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(autofill::ADDRESS_HOME_SORTING_CODE)),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(autofill::ADDRESS_HOME_COUNTRY)),
      ConvertUTF16ToJavaString(env, country.name()),
      ConvertUTF16ToJavaString(
          env, profile.GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER)),
      ConvertUTF16ToJavaString(env,
                               profile.GetRawInfo(autofill::EMAIL_ADDRESS)),
      ConvertUTF8ToJavaString(env, profile.language_code()));
}

base::android::ScopedJavaLocalRef<jobject> CreateFastCheckoutCreditCard(
    JNIEnv* env,
    const autofill::CreditCard& credit_card,
    const std::string& locale) {
  const autofill::data_util::PaymentRequestData& payment_request_data =
      autofill::data_util::GetPaymentRequestData(credit_card.network());
  return Java_FastCheckoutCreditCard_Constructor(
      env, ConvertUTF8ToJavaString(env, credit_card.guid()),
      ConvertUTF8ToJavaString(env, credit_card.origin()),
      credit_card.record_type() == autofill::CreditCard::LOCAL_CARD,
      credit_card.record_type() == autofill::CreditCard::FULL_SERVER_CARD,
      ConvertUTF16ToJavaString(
          env, credit_card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL)),
      ConvertUTF16ToJavaString(
          env, credit_card.GetRawInfo(autofill::CREDIT_CARD_NUMBER)),
      ConvertUTF16ToJavaString(env, credit_card.NetworkAndLastFourDigits()),
      ConvertUTF16ToJavaString(
          env, credit_card.GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH)),
      ConvertUTF16ToJavaString(
          env, credit_card.GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR)),
      ConvertUTF8ToJavaString(env,
                              payment_request_data.basic_card_issuer_network),
      ConvertUTF8ToJavaString(
          env, credit_card.CardIconStringForAutofillSuggestion()),
      ConvertUTF8ToJavaString(env, credit_card.billing_address_id()),
      ConvertUTF8ToJavaString(env, credit_card.server_id()),
      credit_card.instrument_id(),
      ConvertUTF16ToJavaString(env, credit_card.nickname()),
      url::GURLAndroid::FromNativeGURL(env, credit_card.card_art_url()),
      static_cast<jint>(credit_card.virtual_card_enrollment_state()),
      ConvertUTF16ToJavaString(env, credit_card.product_description()));
}
