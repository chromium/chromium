// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_MANIFEST_PARSER_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_MANIFEST_PARSER_ANDROID_H_

#include <jni.h>
#include <memory>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "components/payments/content/utility/payment_manifest_parser.h"

namespace payments {

class ErrorLogger;

// Android wrapper for the host of the utility process that parses manifest
// contents.
class PaymentManifestParserAndroid {
 public:
  explicit PaymentManifestParserAndroid(std::unique_ptr<ErrorLogger> log);
  ~PaymentManifestParserAndroid();

  void ParsePaymentMethodManifest(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jcontent,
      const base::android::JavaParamRef<jobject>& jcallback);

  void ParseWebAppManifest(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jcontent,
      const base::android::JavaParamRef<jobject>& jcallback);

  void DestroyPaymentManifestParserAndroid(JNIEnv* env);

 private:
  PaymentManifestParser parser_;

  DISALLOW_COPY_AND_ASSIGN(PaymentManifestParserAndroid);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_MANIFEST_PARSER_ANDROID_H_
