// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_FAST_CHECKOUT_UI_VIEW_ANDROID_UTILS_H_
#define CHROME_BROWSER_UI_ANDROID_FAST_CHECKOUT_UI_VIEW_ANDROID_UTILS_H_

#include "base/android/jni_android.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

// Creates an FastCheckoutAutofillProfile in Java. This is comparable to
// AutofillProfile::CreateJavaObject.
base::android::ScopedJavaLocalRef<jobject> CreateFastCheckoutAutofillProfile(
    JNIEnv* env,
    const autofill::AutofillProfile& profile,
    const std::string& locale);

// Creates an FastCheckoutCreditCard in Java. This is comparable to
// PersonalDataManagerAndroid::CreateJavaCreditCardFromNative.
base::android::ScopedJavaLocalRef<jobject> CreateFastCheckoutCreditCard(
    JNIEnv* env,
    const autofill::CreditCard& credit_card,
    const std::string& locale);

// Populate the AutofillProfile from the Java FastCheckoutAutofillProfile. This
// is comparable to PersonalDataManagerAndroid::PopulateNativeProfileFromJava.
std::unique_ptr<autofill::AutofillProfile>
CreateFastCheckoutAutofillProfileFromJava(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jprofile,
    const std::string& locale);

// Populate the CreditCard from the Java FastCheckoutCreditCard. This is
// comparable to PersonalDataManagerAndroid::PopulateNativeCreditCardFromJava.
std::unique_ptr<autofill::CreditCard> CreateFastCheckoutCreditCardFromJava(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcredit_card);

#endif  // CHROME_BROWSER_UI_ANDROID_FAST_CHECKOUT_UI_VIEW_ANDROID_UTILS_H_
