// Copyright 2022 The Chromium Authors. All rights reserved.
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
        ConvertUTF8ToJavaString(env, info->country_code));
  }

  Java_ShoppingService_runProductInfoCallback(
      env, callback, url::GURLAndroid::FromNativeGURL(env, url),
      info_java_object);
}

}  // namespace commerce
