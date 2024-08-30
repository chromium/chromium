// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_manifest_web_data_service_android.h"

#include <string>
#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata_services/web_data_service_wrapper_factory.h"
#include "content/public/browser/web_contents.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/payments/content/android/jni_headers/PaymentManifestWebDataService_jni.h"

namespace payments {

PaymentManifestWebDataServiceAndroid::PaymentManifestWebDataServiceAndroid(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    content::WebContents* web_contents)
    : web_contents_(web_contents->GetWeakPtr()), weak_java_obj_(env, obj) {}

PaymentManifestWebDataServiceAndroid::~PaymentManifestWebDataServiceAndroid() =
    default;

void PaymentManifestWebDataServiceAndroid::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  if (!result) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  if (weak_java_obj_.get(env).is_null()) {
    return;
  }

  if (web_data_service_requests_.find(h) == web_data_service_requests_.end()) {
    return;
  }

  switch (result->GetType()) {
    case PAYMENT_WEB_APP_MANIFEST:
      OnWebAppManifestRequestDone(env, h, std::move(result));
      break;
    case PAYMENT_METHOD_MANIFEST:
      OnPaymentMethodManifestRequestDone(env, h, std::move(result));
      break;
    default:
      NOTREACHED_IN_MIGRATION() << "unsupported data type";
  }
}

void PaymentManifestWebDataServiceAndroid::OnWebAppManifestRequestDone(
    JNIEnv* env,
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(result);

  const WDResult<std::vector<WebAppManifestSection>>* typed_result =
      static_cast<const WDResult<std::vector<WebAppManifestSection>>*>(
          result.get());
  const std::vector<WebAppManifestSection>* manifest =
      &(typed_result->GetValue());

  base::android::ScopedJavaLocalRef<jobjectArray> jmanifest =
      Java_PaymentManifestWebDataService_createManifest(env, manifest->size());

  for (size_t i = 0; i < manifest->size(); ++i) {
    const WebAppManifestSection& section = manifest->at(i);
    DCHECK_GE(100U, section.fingerprints.size());

    Java_PaymentManifestWebDataService_addSectionToManifest(
        env, jmanifest, base::checked_cast<int>(i),
        base::android::ConvertUTF8ToJavaString(env, section.id),
        section.min_version,
        base::checked_cast<int>(section.fingerprints.size()));

    for (size_t j = 0; j < section.fingerprints.size(); ++j) {
      const std::vector<uint8_t>& fingerprint = section.fingerprints[j];
      Java_PaymentManifestWebDataService_addFingerprintToSection(
          env, jmanifest, base::checked_cast<int>(i),
          base::checked_cast<int>(j),
          base::android::ToJavaByteArray(env, fingerprint));
    }
  }

  Java_PaymentManifestWebDataServiceCallback_onPaymentWebAppManifestFetched(
      env, *web_data_service_requests_[h], jmanifest);
  web_data_service_requests_.erase(h);
}

void PaymentManifestWebDataServiceAndroid::OnPaymentMethodManifestRequestDone(
    JNIEnv* env,
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(result);

  const WDResult<std::vector<std::string>>* typed_result =
      static_cast<const WDResult<std::vector<std::string>>*>(result.get());
  const std::vector<std::string>* web_apps_ids = &(typed_result->GetValue());

  Java_PaymentManifestWebDataServiceCallback_onPaymentMethodManifestFetched(
      env, *web_data_service_requests_[h],
      base::android::ToJavaArrayOfStrings(env, *web_apps_ids));
  web_data_service_requests_.erase(h);
}

void PaymentManifestWebDataServiceAndroid::Destroy(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj) {
  scoped_refptr<payments::PaymentManifestWebDataService> web_data_service =
      GetPaymentManifestWebDataService();
  if (web_data_service) {
    for (const auto& request : web_data_service_requests_) {
      web_data_service->CancelRequest(request.first);
    }
    web_data_service_requests_.clear();
  }

  delete this;
}

void PaymentManifestWebDataServiceAndroid::AddPaymentMethodManifest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jstring>& jmethod_name,
    const base::android::JavaParamRef<jobjectArray>& japps_package_names) {
  std::vector<std::string> apps_package_names;
  base::android::AppendJavaStringArrayToStringVector(env, japps_package_names,
                                                     &apps_package_names);

  scoped_refptr<payments::PaymentManifestWebDataService> web_data_service =
      GetPaymentManifestWebDataService();
  if (web_data_service == nullptr) {
    return;
  }

  web_data_service->AddPaymentMethodManifest(
      base::android::ConvertJavaStringToUTF8(jmethod_name),
      std::move(apps_package_names));
}

void PaymentManifestWebDataServiceAndroid::AddPaymentWebAppManifest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jobjectArray>& jmanifest_sections) {
  scoped_refptr<payments::PaymentManifestWebDataService> web_data_service =
      GetPaymentManifestWebDataService();
  if (web_data_service == nullptr) {
    return;
  }

  std::vector<WebAppManifestSection> manifest;

  for (auto jsection : jmanifest_sections.ReadElements<jobject>()) {
    WebAppManifestSection section;

    section.id = base::android::ConvertJavaStringToUTF8(
        Java_PaymentManifestWebDataService_getIdFromSection(env, jsection));
    section.min_version = static_cast<int64_t>(
        Java_PaymentManifestWebDataService_getMinVersionFromSection(env,
                                                                    jsection));

    base::android::ScopedJavaLocalRef<jobjectArray> jsection_fingerprints(
        Java_PaymentManifestWebDataService_getFingerprintsFromSection(
            env, jsection));
    for (auto jfingerprint : jsection_fingerprints.ReadElements<jbyteArray>()) {
      std::vector<uint8_t> fingerprint;
      base::android::JavaByteArrayToByteVector(env, jfingerprint, &fingerprint);
      section.fingerprints.emplace_back(fingerprint);
    }

    manifest.emplace_back(std::move(section));
  }

  web_data_service->AddPaymentWebAppManifest(std::move(manifest));
}

bool PaymentManifestWebDataServiceAndroid::GetPaymentMethodManifest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jstring>& jmethod_name,
    const base::android::JavaParamRef<jobject>& jcallback) {
  scoped_refptr<payments::PaymentManifestWebDataService> web_data_service =
      GetPaymentManifestWebDataService();
  if (web_data_service == nullptr) {
    return false;
  }

  WebDataServiceBase::Handle handle =
      web_data_service->GetPaymentMethodManifest(
          base::android::ConvertJavaStringToUTF8(env, jmethod_name), this);
  web_data_service_requests_[handle] =
      std::make_unique<base::android::ScopedJavaGlobalRef<jobject>>(jcallback);

  return true;
}

bool PaymentManifestWebDataServiceAndroid::GetPaymentWebAppManifest(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& unused_obj,
    const base::android::JavaParamRef<jstring>& japp_package_name,
    const base::android::JavaParamRef<jobject>& jcallback) {
  scoped_refptr<payments::PaymentManifestWebDataService> web_data_service =
      GetPaymentManifestWebDataService();
  if (web_data_service == nullptr) {
    return false;
  }

  WebDataServiceBase::Handle handle =
      web_data_service->GetPaymentWebAppManifest(
          base::android::ConvertJavaStringToUTF8(env, japp_package_name), this);
  web_data_service_requests_[handle] =
      std::make_unique<base::android::ScopedJavaGlobalRef<jobject>>(jcallback);

  return true;
}

static jlong JNI_PaymentManifestWebDataService_Init(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  if (!web_contents) {
    return 0;
  }

  PaymentManifestWebDataServiceAndroid* manifest_web_data_service_android =
      new PaymentManifestWebDataServiceAndroid(env, obj, web_contents);
  return reinterpret_cast<intptr_t>(manifest_web_data_service_android);
}

scoped_refptr<PaymentManifestWebDataService>
PaymentManifestWebDataServiceAndroid::GetPaymentManifestWebDataService() {
  if (!web_contents_ || !web_contents_->GetBrowserContext()) {
    return nullptr;
  }

  return webdata_services::WebDataServiceWrapperFactory::
      GetPaymentManifestWebDataServiceForBrowserContext(
          web_contents_->GetBrowserContext(),
          ServiceAccessType::EXPLICIT_ACCESS);
}

}  // namespace payments
