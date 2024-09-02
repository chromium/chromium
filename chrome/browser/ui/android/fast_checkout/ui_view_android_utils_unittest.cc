// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/fast_checkout/ui_view_android_utils.h"

#include "base/android/jni_android.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(FastCheckoutUIViewAndroidUtils, CreateFastCheckoutAutofillProfile) {
  JNIEnv* env = base::android::AttachCurrentThread();
  autofill::AutofillProfile profile = autofill::test::GetFullProfile();

  base::android::ScopedJavaLocalRef<jobject> scoped_profile =
      CreateFastCheckoutAutofillProfile(env, profile, "en-US");

  base::android::JavaParamRef<jobject> java_profile(env, scoped_profile.obj());

  std::unique_ptr<autofill::AutofillProfile> parsed_profile =
      CreateFastCheckoutAutofillProfileFromJava(env, java_profile, "en-US");

  EXPECT_EQ(profile.guid(), parsed_profile->guid());
  EXPECT_EQ(profile.language_code(), parsed_profile->language_code());

  const autofill::FieldType types[] = {
      autofill::NAME_FULL,
      autofill::NAME_FIRST,
      autofill::NAME_MIDDLE,
      autofill::NAME_LAST,
      autofill::NAME_LAST_FIRST,
      autofill::NAME_LAST_SECOND,
      autofill::NAME_LAST_CONJUNCTION,
      autofill::COMPANY_NAME,
      autofill::ADDRESS_HOME_STREET_ADDRESS,
      autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
      autofill::ADDRESS_HOME_CITY,
      autofill::ADDRESS_HOME_STATE,
      autofill::ADDRESS_HOME_ZIP,
      autofill::ADDRESS_HOME_SORTING_CODE,
      autofill::ADDRESS_HOME_COUNTRY,
      autofill::EMAIL_ADDRESS,
      autofill::PHONE_HOME_WHOLE_NUMBER,
  };

  for (autofill::FieldType type : types) {
    EXPECT_EQ(profile.GetRawInfo(type), parsed_profile->GetRawInfo(type));
  }
}

TEST(FastCheckoutUIViewAndroidUtils, CreateFastCheckoutCreditCard) {
  JNIEnv* env = base::android::AttachCurrentThread();
  const autofill::CreditCard credit_cards[] = {
      autofill::test::GetCreditCard(), autofill::test::GetMaskedServerCard()};

  for (const autofill::CreditCard& credit_card : credit_cards) {
    base::android::ScopedJavaLocalRef<jobject> scoped_credit_card =
        CreateFastCheckoutCreditCard(env, credit_card, "en-US");

    base::android::JavaParamRef<jobject> java_credit_card(
        env, scoped_credit_card.obj());

    std::unique_ptr<autofill::CreditCard> parsed_credit_card =
        CreateFastCheckoutCreditCardFromJava(env, java_credit_card);
    EXPECT_EQ(credit_card, *parsed_credit_card.get());
  }
}

}  // namespace
