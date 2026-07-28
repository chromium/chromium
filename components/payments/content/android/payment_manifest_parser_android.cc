// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_manifest_parser_android.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "components/payments/content/developer_console_logger.h"
#include "components/payments/core/error_logger.h"
#include "content/public/browser/web_contents.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/PaymentManifestParser_jni.h"

namespace payments {

PaymentManifestParserAndroid::PaymentManifestParserAndroid(
    std::unique_ptr<ErrorLogger> log)
    : parser_(std::move(log)) {}

PaymentManifestParserAndroid::~PaymentManifestParserAndroid() = default;

base::android::ScopedJavaLocalRef<jobject>
PaymentManifestParserAndroid::ParsePaymentMethodManifest(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jmanifest_url,
    const base::android::JavaRef<jstring>& jcontent) {
  GURL manifest_url = url::GURLAndroid::ToNativeGURL(env, jmanifest_url);
  std::string content = base::android::ConvertJavaStringToUTF8(env, jcontent);

  std::vector<GURL> web_app_manifest_urls;
  std::vector<url::Origin> supported_origins;

  bool success = parser_.ParsePaymentMethodManifest(
      manifest_url, content, &web_app_manifest_urls, &supported_origins);

  if (!success ||
      (web_app_manifest_urls.empty() && supported_origins.empty())) {
    return nullptr;
  }

  DCHECK_GE(100U, web_app_manifest_urls.size());
  DCHECK_GE(100000U, supported_origins.size());

  base::android::ScopedJavaLocalRef<jobjectArray> jurls =
      Java_PaymentManifestParser_createUrlArray(env,
                                                web_app_manifest_urls.size());

  for (size_t i = 0; i < web_app_manifest_urls.size(); ++i) {
    bool is_valid_uri = Java_PaymentManifestParser_addUrl(
        env, jurls, base::checked_cast<int>(i),
        base::android::ConvertUTF8ToJavaString(
            env, web_app_manifest_urls[i].spec()));
    DCHECK(is_valid_uri);
  }

  base::android::ScopedJavaLocalRef<jobjectArray> jorigins =
      Java_PaymentManifestParser_createUrlArray(env, supported_origins.size());

  for (size_t i = 0; i < supported_origins.size(); ++i) {
    bool is_valid_uri = Java_PaymentManifestParser_addUrl(
        env, jorigins, base::checked_cast<int>(i),
        base::android::ConvertUTF8ToJavaString(
            env, supported_origins[i].Serialize()));
    DCHECK(is_valid_uri);
  }

  return Java_PaymentManifestParser_createPaymentMethodManifest(env, jurls,
                                                                jorigins);
}

base::android::ScopedJavaLocalRef<jobjectArray>
PaymentManifestParserAndroid::ParseWebAppManifest(
    JNIEnv* env,
    const base::android::JavaRef<jstring>& jcontent) {
  std::string content = base::android::ConvertJavaStringToUTF8(env, jcontent);

  std::vector<WebAppManifestSection> manifest =
      parser_.ParseWebAppManifest(content);

  if (manifest.empty()) {
    return nullptr;
  }

  DCHECK_GE(100U, manifest.size());

  base::android::ScopedJavaLocalRef<jobjectArray> jmanifest =
      Java_PaymentManifestParser_createManifest(env, manifest.size());

  for (size_t i = 0; i < manifest.size(); ++i) {
    const WebAppManifestSection& section = manifest[i];
    DCHECK_GE(100U, section.fingerprints.size());

    Java_PaymentManifestParser_addSectionToManifest(
        env, jmanifest, base::checked_cast<int>(i),
        base::android::ConvertUTF8ToJavaString(env, section.id),
        section.min_version,
        base::checked_cast<int>(section.fingerprints.size()));

    for (size_t j = 0; j < section.fingerprints.size(); ++j) {
      const std::vector<uint8_t>& fingerprint = section.fingerprints[j];
      Java_PaymentManifestParser_addFingerprintToSection(
          env, jmanifest, base::checked_cast<int>(i),
          base::checked_cast<int>(j),
          base::android::ToJavaByteArray(env, fingerprint));
    }
  }

  return jmanifest;
}

void PaymentManifestParserAndroid::DestroyPaymentManifestParserAndroid(
    JNIEnv* env) {
  delete this;
}

static int64_t JNI_PaymentManifestParser_CreatePaymentManifestParserAndroid(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  auto log = web_contents
                 ? std::make_unique<DeveloperConsoleLogger>(web_contents)
                 : std::make_unique<ErrorLogger>();
  return reinterpret_cast<int64_t>(
      new PaymentManifestParserAndroid(std::move(log)));
}

}  // namespace payments

DEFINE_JNI(PaymentManifestParser)
