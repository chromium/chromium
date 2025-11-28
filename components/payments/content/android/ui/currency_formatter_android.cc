// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/ui/currency_formatter_android.h"

#include <memory>
#include <string>

#include "base/android/jni_string.h"
#include "components/payments/core/currency_formatter.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/ui/jni_headers/CurrencyFormatter_jni.h"

namespace payments {
namespace {

using ::base::android::JavaParamRef;
using ::base::android::ConvertJavaStringToUTF8;

}  // namespace

CurrencyFormatterAndroid::CurrencyFormatterAndroid(
    JNIEnv* env,
    const JavaParamRef<jstring>& currency_code,
    const JavaParamRef<jstring>& locale_name) {
  currency_formatter_ = std::make_unique<CurrencyFormatter>(
      ConvertJavaStringToUTF8(env, currency_code),
      ConvertJavaStringToUTF8(env, locale_name));
}

CurrencyFormatterAndroid::~CurrencyFormatterAndroid() = default;

void CurrencyFormatterAndroid::Destroy(JNIEnv* env) {
  delete this;
}

void CurrencyFormatterAndroid::SetMaxFractionalDigits(
    JNIEnv* env,
    jint jmax_fractional_digits) {
  currency_formatter_->SetMaxFractionalDigits(jmax_fractional_digits);
}

base::android::ScopedJavaLocalRef<jstring> CurrencyFormatterAndroid::Format(
    JNIEnv* env,
    const JavaParamRef<jstring>& amount) {
  std::u16string result =
      currency_formatter_->Format(ConvertJavaStringToUTF8(env, amount));
  return base::android::ConvertUTF16ToJavaString(env, result);
}

base::android::ScopedJavaLocalRef<jstring>
CurrencyFormatterAndroid::GetFormattedCurrencyCode(JNIEnv* env) {
  return base::android::ConvertUTF8ToJavaString(
      env, currency_formatter_->formatted_currency_code());
}

static jlong JNI_CurrencyFormatter_InitCurrencyFormatterAndroid(
    JNIEnv* env,
    const JavaParamRef<jstring>& currency_code,
    const JavaParamRef<jstring>& locale_name) {
  CurrencyFormatterAndroid* currency_formatter_android =
      new CurrencyFormatterAndroid(env, currency_code, locale_name);
  return reinterpret_cast<intptr_t>(currency_formatter_android);
}

}  // namespace payments

DEFINE_JNI(CurrencyFormatter)
