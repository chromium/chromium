// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/android/shopping_service_android.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "components/commerce/core/shopping_service_jni_headers/ShoppingService_jni.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::RunBooleanCallbackAndroid;
using base::android::ScopedJavaLocalRef;

namespace commerce {

ShoppingServiceAndroid::ShoppingServiceAndroid(ShoppingService* service)
    : shopping_service_(service), weak_ptr_factory_(this) {
  java_ref_.Reset(Java_ShoppingService_create(
      base::android::AttachCurrentThread(), reinterpret_cast<jlong>(this)));
  scoped_subscriptions_observer_.Observe(shopping_service_);
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

void ShoppingServiceAndroid::Subscribe(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint j_type,
    jint j_id_type,
    jint j_management_type,
    const JavaParamRef<jstring>& j_id,
    const JavaParamRef<jstring>& j_seen_offer_id,
    jlong j_seen_price,
    const JavaParamRef<jstring>& j_seen_country,
    const JavaParamRef<jobject>& j_callback) {
  std::string id = ConvertJavaStringToUTF8(j_id);
  std::string seen_offer_id = ConvertJavaStringToUTF8(j_seen_offer_id);
  std::string seen_country = ConvertJavaStringToUTF8(j_seen_country);
  CHECK(!id.empty());

  auto user_seen_offer = absl::make_optional<UserSeenOffer>(
      seen_offer_id, j_seen_price, seen_country);
  CommerceSubscription sub(SubscriptionType(j_type), IdentifierType(j_id_type),
                           id, ManagementType(j_management_type),
                           kUnknownSubscriptionTimestamp,
                           std::move(user_seen_offer));
  std::unique_ptr<std::vector<CommerceSubscription>> subs =
      std::make_unique<std::vector<CommerceSubscription>>();
  subs->push_back(std::move(sub));

  auto callback = base::BindOnce(&RunBooleanCallbackAndroid,
                                 ScopedJavaGlobalRef<jobject>(j_callback));

  shopping_service_->Subscribe(std::move(subs), std::move(callback));
}

void ShoppingServiceAndroid::Unsubscribe(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint j_type,
    jint j_id_type,
    jint j_management_type,
    const JavaParamRef<jstring>& j_id,
    const JavaParamRef<jobject>& j_callback) {
  std::string id = ConvertJavaStringToUTF8(j_id);
  CHECK(!id.empty());

  CommerceSubscription sub(SubscriptionType(j_type), IdentifierType(j_id_type),
                           id, ManagementType(j_management_type),
                           kUnknownSubscriptionTimestamp, absl::nullopt);
  std::unique_ptr<std::vector<CommerceSubscription>> subs =
      std::make_unique<std::vector<CommerceSubscription>>();
  subs->push_back(std::move(sub));

  auto callback = base::BindOnce(&RunBooleanCallbackAndroid,
                                 ScopedJavaGlobalRef<jobject>(j_callback));

  shopping_service_->Unsubscribe(std::move(subs), std::move(callback));
}

void ShoppingServiceAndroid::IsSubscribed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint j_type,
    jint j_id_type,
    jint j_management_type,
    const JavaParamRef<jstring>& j_id,
    const JavaParamRef<jobject>& j_callback) {
  std::string id = ConvertJavaStringToUTF8(j_id);
  CHECK(!id.empty());

  CommerceSubscription sub(SubscriptionType(j_type), IdentifierType(j_id_type),
                           id, ManagementType(j_management_type),
                           kUnknownSubscriptionTimestamp, absl::nullopt);

  shopping_service_->IsSubscribed(
      std::move(sub),
      base::BindOnce(
          [](const ScopedJavaGlobalRef<jobject>& callback, bool is_tracked) {
            base::android::RunBooleanCallbackAndroid(callback, is_tracked);
          },
          ScopedJavaGlobalRef<jobject>(j_callback)));
}

bool ShoppingServiceAndroid::IsSubscribedFromCache(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint j_type,
    jint j_id_type,
    jint j_management_type,
    const JavaParamRef<jstring>& j_id) {
  std::string id = ConvertJavaStringToUTF8(j_id);
  CHECK(!id.empty());

  CommerceSubscription sub(SubscriptionType(j_type), IdentifierType(j_id_type),
                           id, ManagementType(j_management_type),
                           kUnknownSubscriptionTimestamp, absl::nullopt);

  return shopping_service_->IsSubscribedFromCache(std::move(sub));
}

void ShoppingServiceAndroid::OnSubscribe(
    const std::vector<CommerceSubscription>& subscriptions,
    bool succeeded) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ShoppingService_onSubscribe(
      env, java_ref_, ConvertSubscriptionsToJavaList(subscriptions), succeeded);
}

void ShoppingServiceAndroid::OnUnsubscribe(
    const std::vector<CommerceSubscription>& subscriptions,
    bool succeeded) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ShoppingService_onUnsubscribe(
      env, java_ref_, ConvertSubscriptionsToJavaList(subscriptions), succeeded);
}

ScopedJavaLocalRef<jobject>
ShoppingServiceAndroid::ConvertSubscriptionsToJavaList(
    const std::vector<CommerceSubscription>& subscriptions) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_subs = nullptr;
  for (const auto& sub : subscriptions) {
    j_subs = Java_ShoppingService_createSubscriptionAndAddToList(
        env, java_ref_, j_subs, static_cast<int>(sub.type),
        static_cast<int>(sub.id_type), static_cast<int>(sub.management_type),
        ConvertUTF8ToJavaString(env, sub.id));
  }
  return j_subs;
}

bool ShoppingServiceAndroid::IsShoppingListEligible(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CHECK(shopping_service_);

  return shopping_service_->IsShoppingListEligible();
}

bool ShoppingServiceAndroid::IsMerchantViewerEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CHECK(shopping_service_);

  return shopping_service_->IsMerchantViewerEnabled();
}

}  // namespace commerce
