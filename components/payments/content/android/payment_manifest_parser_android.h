// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_MANIFEST_PARSER_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_MANIFEST_PARSER_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "components/payments/content/utility/payment_manifest_parser.h"

namespace payments {

class ErrorLogger;

// Android wrapper for parsing manifest contents.
class PaymentManifestParserAndroid {
 public:
  explicit PaymentManifestParserAndroid(std::unique_ptr<ErrorLogger> log);

  PaymentManifestParserAndroid(const PaymentManifestParserAndroid&) = delete;
  PaymentManifestParserAndroid& operator=(const PaymentManifestParserAndroid&) =
      delete;

  ~PaymentManifestParserAndroid();

  base::android::ScopedJavaLocalRef<jobject> ParsePaymentMethodManifest(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jmanifest_url,
      const base::android::JavaRef<jstring>& jcontent);

  base::android::ScopedJavaLocalRef<jobjectArray> ParseWebAppManifest(
      JNIEnv* env,
      const base::android::JavaRef<jstring>& jcontent);

  void DestroyPaymentManifestParserAndroid(JNIEnv* env);

 private:
  PaymentManifestParser parser_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_MANIFEST_PARSER_ANDROID_H_
