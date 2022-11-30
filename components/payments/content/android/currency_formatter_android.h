// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_CURRENCY_FORMATTER_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_CURRENCY_FORMATTER_ANDROID_H_

#include <jni.h>
#include <memory>

#include "base/android/scoped_java_ref.h"

namespace payments {

class CurrencyFormatter;

// Forwarding calls to payments::CurrencyFormatter.
class CurrencyFormatterAndroid {
 public:
  CurrencyFormatterAndroid(
      JNIEnv* env,
      jobject jcaller,
      const base::android::JavaParamRef<jstring>& currency_code,
      const base::android::JavaParamRef<jstring>& locale_name);

  CurrencyFormatterAndroid(const CurrencyFormatterAndroid&) = delete;
  CurrencyFormatterAndroid& operator=(const CurrencyFormatterAndroid&) = delete;

  ~CurrencyFormatterAndroid();

  // Message from Java to destroy this object.
  void Destroy(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& jcaller);

  // Set the maximum number of fractional digits.
  void SetMaxFractionalDigits(JNIEnv* env, jint jnum_fractional_digits);

  // Refer to CurrencyFormatter::Format documentation.
  base::android::ScopedJavaLocalRef<jstring> Format(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller,
      const base::android::JavaParamRef<jstring>& amount);

  base::android::ScopedJavaLocalRef<jstring> GetFormattedCurrencyCode(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& jcaller);

 private:
  std::unique_ptr<CurrencyFormatter> currency_formatter_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_CURRENCY_FORMATTER_ANDROID_H_
