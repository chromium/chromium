// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/fast_checkout/ui_view_android_utils.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/android/fast_checkout/jni_headers/FastCheckoutAutofillProfile_jni.h"
#include "chrome/browser/ui/android/fast_checkout/jni_headers/FastCheckoutCreditCard_jni.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "url/android/gurl_android.h"

namespace {
using ::base::android::ConvertUTF16ToJavaString;
using ::base::android::ConvertUTF8ToJavaString;
using ::base::android::JavaRef;

void MaybeSetInfo(autofill::AutofillProfile* profile,
                  autofill::ServerFieldType type,
                  const JavaRef<jstring>& value,
                  const std::string& locale) {
  if (value) {
    profile->SetInfo(type, ConvertJavaStringToUTF16(value), locale);
  }
}

void MaybeSetRawInfo(autofill::AutofillProfile* profile,
                     autofill::ServerFieldType type,
                     const JavaRef<jstring>& value) {
  if (value) {
    profile->SetRawInfo(type, ConvertJavaStringToUTF16(value));
  }
}

}  // namespace

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

std::unique_ptr<autofill::AutofillProfile>
CreateFastCheckoutAutofillProfileFromJava(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile,
    const std::string& locale) {
  auto profile = std::make_unique<autofill::AutofillProfile>();
  // Only set the guid if it is an existing profile (Java guid not empty).
  // Otherwise, keep the generated one.
  std::string guid = ConvertJavaStringToUTF8(
      Java_FastCheckoutAutofillProfile_getGUID(env, jprofile));
  if (!guid.empty()) {
    profile->set_guid(guid);
  }

  profile->set_origin(ConvertJavaStringToUTF8(
      Java_FastCheckoutAutofillProfile_getOrigin(env, jprofile)));
  MaybeSetInfo(profile.get(), autofill::NAME_FULL,
               Java_FastCheckoutAutofillProfile_getFullName(env, jprofile),
               locale);
  MaybeSetRawInfo(
      profile.get(), autofill::NAME_HONORIFIC_PREFIX,
      Java_FastCheckoutAutofillProfile_getHonorificPrefix(env, jprofile));
  MaybeSetRawInfo(
      profile.get(), autofill::COMPANY_NAME,
      Java_FastCheckoutAutofillProfile_getCompanyName(env, jprofile));
  MaybeSetRawInfo(
      profile.get(), autofill::ADDRESS_HOME_STREET_ADDRESS,
      Java_FastCheckoutAutofillProfile_getStreetAddress(env, jprofile));
  MaybeSetRawInfo(profile.get(), autofill::ADDRESS_HOME_STATE,
                  Java_FastCheckoutAutofillProfile_getRegion(env, jprofile));
  MaybeSetRawInfo(profile.get(), autofill::ADDRESS_HOME_CITY,
                  Java_FastCheckoutAutofillProfile_getLocality(env, jprofile));
  MaybeSetRawInfo(
      profile.get(), autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
      Java_FastCheckoutAutofillProfile_getDependentLocality(env, jprofile));
  MaybeSetRawInfo(
      profile.get(), autofill::ADDRESS_HOME_ZIP,
      Java_FastCheckoutAutofillProfile_getPostalCode(env, jprofile));
  MaybeSetRawInfo(
      profile.get(), autofill::ADDRESS_HOME_SORTING_CODE,
      Java_FastCheckoutAutofillProfile_getSortingCode(env, jprofile));
  MaybeSetInfo(profile.get(), autofill::ADDRESS_HOME_COUNTRY,
               Java_FastCheckoutAutofillProfile_getCountryCode(env, jprofile),
               locale);
  MaybeSetRawInfo(
      profile.get(), autofill::PHONE_HOME_WHOLE_NUMBER,
      Java_FastCheckoutAutofillProfile_getPhoneNumber(env, jprofile));
  MaybeSetRawInfo(
      profile.get(), autofill::EMAIL_ADDRESS,
      Java_FastCheckoutAutofillProfile_getEmailAddress(env, jprofile));
  profile->set_language_code(ConvertJavaStringToUTF8(
      Java_FastCheckoutAutofillProfile_getLanguageCode(env, jprofile)));
  profile->FinalizeAfterImport();
  return profile;
}

std::unique_ptr<autofill::CreditCard> CreateFastCheckoutCreditCardFromJava(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcredit_card) {
  auto credit_card = std::make_unique<autofill::CreditCard>();
  // Only set the guid if it is an existing card (java guid not empty).
  // Otherwise, keep the generated one.
  std::string guid = ConvertJavaStringToUTF8(
      Java_FastCheckoutCreditCard_getGUID(env, jcredit_card));
  if (!guid.empty()) {
    credit_card->set_guid(guid);
  }

  if (Java_FastCheckoutCreditCard_getIsLocal(env, jcredit_card)) {
    credit_card->set_record_type(autofill::CreditCard::LOCAL_CARD);
  } else {
    if (Java_FastCheckoutCreditCard_getIsCached(env, jcredit_card)) {
      credit_card->set_record_type(autofill::CreditCard::FULL_SERVER_CARD);
    } else {
      credit_card->set_record_type(autofill::CreditCard::MASKED_SERVER_CARD);
      credit_card->SetNetworkForMaskedCard(
          autofill::data_util::GetIssuerNetworkForBasicCardIssuerNetwork(
              ConvertJavaStringToUTF8(
                  env, Java_FastCheckoutCreditCard_getBasicCardIssuerNetwork(
                           env, jcredit_card))));
    }
  }

  credit_card->set_origin(ConvertJavaStringToUTF8(
      Java_FastCheckoutCreditCard_getOrigin(env, jcredit_card)));
  credit_card->SetRawInfo(
      autofill::CREDIT_CARD_NAME_FULL,
      ConvertJavaStringToUTF16(
          Java_FastCheckoutCreditCard_getName(env, jcredit_card)));
  credit_card->SetRawInfo(
      autofill::CREDIT_CARD_NUMBER,
      ConvertJavaStringToUTF16(
          Java_FastCheckoutCreditCard_getNumber(env, jcredit_card)));
  credit_card->SetRawInfo(
      autofill::CREDIT_CARD_EXP_MONTH,
      ConvertJavaStringToUTF16(
          Java_FastCheckoutCreditCard_getMonth(env, jcredit_card)));
  credit_card->SetRawInfo(
      autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
      ConvertJavaStringToUTF16(
          Java_FastCheckoutCreditCard_getYear(env, jcredit_card)));
  credit_card->set_billing_address_id(ConvertJavaStringToUTF8(
      Java_FastCheckoutCreditCard_getBillingAddressId(env, jcredit_card)));
  credit_card->set_server_id(ConvertJavaStringToUTF8(
      Java_FastCheckoutCreditCard_getServerId(env, jcredit_card)));
  credit_card->set_instrument_id(
      Java_FastCheckoutCreditCard_getInstrumentId(env, jcredit_card));
  credit_card->SetNickname(ConvertJavaStringToUTF16(
      Java_FastCheckoutCreditCard_getNickname(env, jcredit_card)));
  base::android::ScopedJavaLocalRef<jobject> jcard_art_url =
      Java_FastCheckoutCreditCard_getCardArtUrl(env, jcredit_card);
  if (!jcard_art_url.is_null()) {
    credit_card->set_card_art_url(
        *url::GURLAndroid::ToNativeGURL(env, jcard_art_url));
  }
  credit_card->set_virtual_card_enrollment_state(
      static_cast<autofill::CreditCard::VirtualCardEnrollmentState>(
          Java_FastCheckoutCreditCard_getVirtualCardEnrollmentState(
              env, jcredit_card)));
  credit_card->set_product_description(ConvertJavaStringToUTF16(
      Java_FastCheckoutCreditCard_getProductDescription(env, jcredit_card)));
  return credit_card;
}
