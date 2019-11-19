// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/currency_formatter_android.h"

#include "base/android/jni_string.h"
#include "base/strings/string16.h"
#include "components/payments/content/android/jni_headers/CurrencyFormatter_jni.h"
#include "components/payments/core/currency_formatter.h"

namespace payments {
namespace {

using ::base::android::JavaParamRef;
using ::base::android::ConvertJavaStringToUTF8;

}  // namespace

CurrencyFormatterAndroid::CurrencyFormatterAndroid(
    JNIEnv* env,
    jobject jcaller,
    const JavaParamRef<jstring>& currency_code,
    const JavaParamRef<jstring>& locale_name) {
  currency_formatter_.reset(
      new CurrencyFormatter(ConvertJavaStringToUTF8(env, currency_code),
                            ConvertJavaStringToUTF8(env, locale_name)));
}

CurrencyFormatterAndroid::~CurrencyFormatterAndroid() {}

void CurrencyFormatterAndroid::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& jcaller) {
  delete this;
}

base::android::ScopedJavaLocalRef<jstring> CurrencyFormatterAndroid::Format(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& amount) {
  base::string16 result =
      currency_formatter_->Format(ConvertJavaStringToUTF8(env, amount));
  return base::android::ConvertUTF16ToJavaString(env, result);
}

base::android::ScopedJavaLocalRef<jstring>
CurrencyFormatterAndroid::GetFormattedCurrencyCode(
    JNIEnv* env,
    const JavaParamRef<jobject>& unused_obj) {
  return base::android::ConvertUTF8ToJavaString(
      env, currency_formatter_->formatted_currency_code());
}

static jlong JNI_CurrencyFormatter_InitCurrencyFormatterAndroid(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& currency_code,
    const JavaParamRef<jstring>& locale_name) {
  CurrencyFormatterAndroid* currency_formatter_android =
      new CurrencyFormatterAndroid(env, obj, currency_code, locale_name);
  return reinterpret_cast<intptr_t>(currency_formatter_android);
}

}  // namespace payments
