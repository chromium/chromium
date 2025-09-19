// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_WEB_PAYMENTS_WEB_DATA_SERVICE_ANDROID_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_WEB_PAYMENTS_WEB_DATA_SERVICE_ANDROID_H_

#include <map>
#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/web_payments_web_data_service.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace content {
class WebContents;
}

namespace payments {

// Android wrapper of the WebPaymentsWebDataService which provides access
// from the Java layer.
class WebPaymentsWebDataServiceAndroid {
 public:
  WebPaymentsWebDataServiceAndroid(JNIEnv* env,
                                   const jni_zero::JavaRef<jobject>& obj,
                                   content::WebContents* web_contents);

  WebPaymentsWebDataServiceAndroid(const WebPaymentsWebDataServiceAndroid&) =
      delete;
  WebPaymentsWebDataServiceAndroid& operator=(
      const WebPaymentsWebDataServiceAndroid&) = delete;

  ~WebPaymentsWebDataServiceAndroid();

  // Destroys this object.
  void Destroy(JNIEnv* env);

  // Adds the supported |japp_package_names| of the |jmethod_name| to the
  // cache.
  void AddPaymentMethodManifest(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jmethod_name,
      const base::android::JavaParamRef<jobjectArray>& japp_package_names);

  // Adds the web app |jmanifest_sections|.
  void AddPaymentWebAppManifest(
      JNIEnv* env,
      const base::android::JavaParamRef<jobjectArray>& jmanifest_sections);

  // Gets the payment |jmethod_name|'s manifest asynchronously from the web data
  // service. Return true if the result will be returned through |jcallback|.
  bool GetPaymentMethodManifest(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& jmethod_name,
      const base::android::JavaParamRef<jobject>& jcallback);

  // Gets the payment |japp_package_name|'s manifest asynchronously from the web
  // data service. Return true if the result will be returned through
  // |jcallback|.
  bool GetPaymentWebAppManifest(
      JNIEnv* env,
      const base::android::JavaParamRef<jstring>& japp_package_name,
      const base::android::JavaParamRef<jobject>& jcallback);

 private:
  void OnWebDataServiceRequestDone(WebDataServiceBase::Handle h,
                                   std::unique_ptr<WDTypedResult> result);
  void OnWebAppManifestRequestDone(JNIEnv* env,
                                   WebDataServiceBase::Handle h,
                                   std::unique_ptr<WDTypedResult> result);
  void OnPaymentMethodManifestRequestDone(
      JNIEnv* env,
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result);
  scoped_refptr<WebPaymentsWebDataService> GetWebPaymentsWebDataService();

  base::WeakPtr<content::WebContents> web_contents_;

  // Pointer to the java counterpart.
  JavaObjectWeakGlobalRef weak_java_obj_;

  // Map of request handle and its correspond callback.
  std::map<WebDataServiceBase::Handle,
           std::unique_ptr<base::android::ScopedJavaGlobalRef<jobject>>>
      web_data_service_requests_;

  base::WeakPtrFactory<WebPaymentsWebDataServiceAndroid> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_WEB_PAYMENTS_WEB_DATA_SERVICE_ANDROID_H_
