// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_MANIFEST_DOWNLOADER_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_MANIFEST_DOWNLOADER_ANDROID_H_

#include <jni.h>

#include <memory>

#include "base/android/jni_android.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_manifest_downloader.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace payments {

class CSPChecker;
class ErrorLogger;

// Android wrapper for the payment manifest downloader.
class PaymentManifestDownloaderAndroid {
 public:
  PaymentManifestDownloaderAndroid(
      std::unique_ptr<ErrorLogger> log,
      base::WeakPtr<CSPChecker> csp_checker,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory_rfh);

  PaymentManifestDownloaderAndroid(const PaymentManifestDownloaderAndroid&) =
      delete;
  PaymentManifestDownloaderAndroid& operator=(
      const PaymentManifestDownloaderAndroid&) = delete;

  ~PaymentManifestDownloaderAndroid();

  void DownloadPaymentMethodManifest(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jmerchant_origin,
      const base::android::JavaRef<jobject>& jurl,
      const base::android::JavaRef<jobject>& jcallback);

  void DownloadWebAppManifest(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& jpayment_method_manifest_origin,
      const base::android::JavaRef<jobject>& jurl,
      const base::android::JavaRef<jobject>& jcallback);

  // Deletes this object.
  void Destroy(JNIEnv* env);

 private:
  PaymentManifestDownloader downloader_;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_MANIFEST_DOWNLOADER_ANDROID_H_
