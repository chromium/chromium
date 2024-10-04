// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_manifest_downloader_android.h"

#include <memory>

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/functional/bind.h"
#include "components/payments/content/android/csp_checker_android.h"
#include "components/payments/content/developer_console_logger.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/PaymentManifestDownloader_jni.h"

namespace payments {
namespace {

class DownloadCallback {
 public:
  explicit DownloadCallback(
      const base::android::JavaParamRef<jobject>& jcallback)
      : jcallback_(jcallback) {}

  DownloadCallback(const DownloadCallback&) = delete;
  DownloadCallback& operator=(const DownloadCallback&) = delete;

  ~DownloadCallback() = default;

  void OnPaymentMethodManifestDownload(const GURL& url_after_redirects,
                                       const std::string& content,
                                       const std::string& error_message) {
    JNIEnv* env = base::android::AttachCurrentThread();

    if (content.empty()) {
      Java_ManifestDownloadCallback_onManifestDownloadFailure(
          env, jcallback_,
          base::android::ConvertUTF8ToJavaString(env, error_message));
    } else {
      Java_ManifestDownloadCallback_onPaymentMethodManifestDownloadSuccess(
          env, jcallback_,
          url::GURLAndroid::FromNativeGURL(env, url_after_redirects),
          url::Origin::Create(url_after_redirects).ToJavaObject(env),
          base::android::ConvertUTF8ToJavaString(env, content));
    }
  }

  void OnWebAppManifestDownload(const GURL& url_after_redirects,
                                const std::string& content,
                                const std::string& error_message) {
    JNIEnv* env = base::android::AttachCurrentThread();

    if (content.empty()) {
      Java_ManifestDownloadCallback_onManifestDownloadFailure(
          env, jcallback_,
          base::android::ConvertUTF8ToJavaString(env, error_message));
    } else {
      Java_ManifestDownloadCallback_onWebAppManifestDownloadSuccess(
          env, jcallback_,
          base::android::ConvertUTF8ToJavaString(env, content));
    }
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> jcallback_;
};

}  // namespace

PaymentManifestDownloaderAndroid::PaymentManifestDownloaderAndroid(
    std::unique_ptr<ErrorLogger> log,
    base::WeakPtr<CSPChecker> csp_checker,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : downloader_(std::move(log), csp_checker, std::move(url_loader_factory)) {}

PaymentManifestDownloaderAndroid::~PaymentManifestDownloaderAndroid() = default;

void PaymentManifestDownloaderAndroid::DownloadPaymentMethodManifest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jmerchant_origin,
    const base::android::JavaParamRef<jobject>& jurl,
    const base::android::JavaParamRef<jobject>& jcallback) {
  downloader_.DownloadPaymentMethodManifest(
      url::Origin::FromJavaObject(env, jmerchant_origin),
      url::GURLAndroid::ToNativeGURL(env, jurl),
      base::BindOnce(&DownloadCallback::OnPaymentMethodManifestDownload,
                     std::make_unique<DownloadCallback>(jcallback)));
}

void PaymentManifestDownloaderAndroid::DownloadWebAppManifest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jpayment_method_manifest_origin,
    const base::android::JavaParamRef<jobject>& jurl,
    const base::android::JavaParamRef<jobject>& jcallback) {
  downloader_.DownloadWebAppManifest(
      url::Origin::FromJavaObject(env, jpayment_method_manifest_origin),
      url::GURLAndroid::ToNativeGURL(env, jurl),
      base::BindOnce(&DownloadCallback::OnWebAppManifestDownload,
                     std::make_unique<DownloadCallback>(jcallback)));
}

void PaymentManifestDownloaderAndroid::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  delete this;
}

// Static free function declared and called directly from java.
// Caller owns the result. Returns 0 on error.
static jlong JNI_PaymentManifestDownloader_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents,
    jlong native_csp_checker_android) {
  if (!jweb_contents || !native_csp_checker_android)
    return 0;

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents)
    return 0;

  return reinterpret_cast<jlong>(new PaymentManifestDownloaderAndroid(
      std::make_unique<DeveloperConsoleLogger>(web_contents),
      payments::CSPCheckerAndroid::GetWeakPtr(native_csp_checker_android),
      web_contents->GetBrowserContext()
          ->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()));
}

// Static free function declared and called directly from java.
static base::android::ScopedJavaLocalRef<jobject>
JNI_PaymentManifestDownloader_CreateOpaqueOriginForTest(JNIEnv* env) {
  return url::Origin().ToJavaObject(env);
}

}  // namespace payments
