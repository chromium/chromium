// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/android/shopping_service_android.h"

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "components/commerce/core/shopping_service_jni_headers/ShoppingService_jni.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace commerce {

ShoppingServiceAndroid::ShoppingServiceAndroid(ShoppingService* service)
    : shopping_service_(service), weak_ptr_factory_(this) {
  java_ref_.Reset(Java_ShoppingService_create(
      base::android::AttachCurrentThread(), reinterpret_cast<jlong>(this)));
}

ShoppingServiceAndroid::~ShoppingServiceAndroid() {
  Java_ShoppingService_destroy(base::android::AttachCurrentThread(), java_ref_);
}

void ShoppingServiceAndroid::GetProductInfoForUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_gurl,
    const JavaParamRef<jobject>& j_callback) {
  CHECK(shopping_service_);

  GURL url = *url::GURLAndroid::ToNativeGURL(env, j_gurl);

  shopping_service_->GetProductInfoForUrl(
      url, base::BindOnce(&ShoppingServiceAndroid::HandleProductInfoCallback,
                          weak_ptr_factory_.GetWeakPtr(), env,
                          ScopedJavaGlobalRef<jobject>(j_callback)));
}

ScopedJavaLocalRef<jobject>
ShoppingServiceAndroid::GetAvailableProductInfoForUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_gurl) {
  CHECK(shopping_service_);

  GURL url = *url::GURLAndroid::ToNativeGURL(env, j_gurl);

  absl::optional<ProductInfo> info =
      shopping_service_->GetAvailableProductInfoForUrl(url);

  ScopedJavaLocalRef<jobject> info_java_object(nullptr);
  if (info.has_value()) {
    info_java_object = Java_ShoppingService_createProductInfo(
        env, ConvertUTF8ToJavaString(env, info->title),
        url::GURLAndroid::FromNativeGURL(env, GURL(info->image_url)),
        info->product_cluster_id, info->offer_id,
        ConvertUTF8ToJavaString(env, info->currency_code), info->amount_micros,
        ConvertUTF8ToJavaString(env, info->country_code),
        info->previous_amount_micros.has_value(),
        info->previous_amount_micros.value_or(0));
  }

  return info_java_object;
}

void ShoppingServiceAndroid::HandleProductInfoCallback(
    JNIEnv* env,
    const ScopedJavaGlobalRef<jobject>& callback,
    const GURL& url,
    const absl::optional<ProductInfo>& info) {
  ScopedJavaLocalRef<jobject> info_java_object(nullptr);
  if (info.has_value()) {
    info_java_object = Java_ShoppingService_createProductInfo(
        env, ConvertUTF8ToJavaString(env, info->title),
        url::GURLAndroid::FromNativeGURL(env, GURL(info->image_url)),
        info->product_cluster_id, info->offer_id,
        ConvertUTF8ToJavaString(env, info->currency_code), info->amount_micros,
        ConvertUTF8ToJavaString(env, info->country_code),
        info->previous_amount_micros.has_value(),
        info->previous_amount_micros.value_or(0));
  }

  Java_ShoppingService_runProductInfoCallback(
      env, callback, url::GURLAndroid::FromNativeGURL(env, url),
      info_java_object);
}

void ShoppingServiceAndroid::GetMerchantInfoForUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_gurl,
    const JavaParamRef<jobject>& j_callback) {
  CHECK(shopping_service_);

  GURL url = *url::GURLAndroid::ToNativeGURL(env, j_gurl);

  shopping_service_->GetMerchantInfoForUrl(
      url, base::BindOnce(&ShoppingServiceAndroid::HandleMerchantInfoCallback,
                          weak_ptr_factory_.GetWeakPtr(), env,
                          ScopedJavaGlobalRef<jobject>(j_callback)));
}

void ShoppingServiceAndroid::HandleMerchantInfoCallback(
    JNIEnv* env,
    const ScopedJavaGlobalRef<jobject>& callback,
    const GURL& url,
    absl::optional<MerchantInfo> info) {
  ScopedJavaLocalRef<jobject> info_java_object(nullptr);
  if (info.has_value()) {
    info_java_object = Java_ShoppingService_createMerchantInfo(
        env, info->star_rating, info->count_rating,
        url::GURLAndroid::FromNativeGURL(env, GURL(info->details_page_url)),
        info->has_return_policy, info->non_personalized_familiarity_score,
        info->contains_sensitive_content, info->proactive_message_disabled);
  }

  Java_ShoppingService_runMerchantInfoCallback(
      env, callback, url::GURLAndroid::FromNativeGURL(env, url),
      info_java_object);
}

void ShoppingServiceAndroid::FetchPriceEmailPref(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CHECK(shopping_service_);

  shopping_service_->FetchPriceEmailPref();
}

void ShoppingServiceAndroid::ScheduleSavedProductUpdate(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CHECK(shopping_service_);

  shopping_service_->ScheduleSavedProductUpdate();
}

}  // namespace commerce
