// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/android/shopping_service_android.h"

#include <optional>

#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/commerce/core/subscriptions/commerce_subscription.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/commerce/core/android/core_jni/DiscountInfo_jni.h"
#include "components/commerce/core/android/core_jni/ShoppingService_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::RunBooleanCallbackAndroid;
using base::android::ScopedJavaLocalRef;

namespace commerce {

namespace {

ScopedJavaLocalRef<jobject> ToJavaObject(JNIEnv* env,
                                         const CommerceSubscription& sub) {
  return Java_ShoppingService_createSubscription(
      env, static_cast<int>(sub.type), static_cast<int>(sub.id_type),
      static_cast<int>(sub.management_type),
      ConvertUTF8ToJavaString(env, sub.id));
}

ScopedJavaLocalRef<jobject> ConvertToJavaDiscountInfo(
    JNIEnv* env,
    const DiscountInfo& info) {
  ScopedJavaLocalRef<jstring> terms_and_conditions_java_string =
      info.terms_and_conditions.has_value()
          ? ConvertUTF8ToJavaString(env, info.terms_and_conditions.value())
          : nullptr;
  ScopedJavaLocalRef<jstring> discount_code_java_string =
      info.discount_code.has_value()
          ? ConvertUTF8ToJavaString(env, info.discount_code.value())
          : nullptr;

  return Java_DiscountInfo_Constructor(
      env, static_cast<int>(info.cluster_type), static_cast<int>(info.type),
      ConvertUTF8ToJavaString(env, info.language_code),
      ConvertUTF8ToJavaString(env, info.description_detail),
      terms_and_conditions_java_string,
      ConvertUTF8ToJavaString(env, info.value_in_text),
      discount_code_java_string, info.id, info.is_merchant_wide,
      info.expiry_time_sec, info.offer_id);
}

ScopedJavaLocalRef<jobjectArray> ConvertToJavaDiscountInfos(
    JNIEnv* env,
    const std::vector<DiscountInfo>& info) {
  std::vector<ScopedJavaLocalRef<jobject>> j_discount_infos;

  jclass discount_info_clazz =
      org_chromium_components_commerce_core_DiscountInfo_clazz(env);

  for (size_t i = 0; i < info.size(); i++) {
    ScopedJavaLocalRef<jobject> discount_info_java =
        ConvertToJavaDiscountInfo(env, info[i]);
    j_discount_infos.push_back(discount_info_java);
  }
  return base::android::ToTypedJavaArrayOfObjects(
      env, base::make_span(j_discount_infos), discount_info_clazz);
}

}  // namespace

ShoppingServiceAndroid::ShoppingServiceAndroid(ShoppingService* service)
    : shopping_service_(service), weak_ptr_factory_(this) {
  java_ref_.Reset(Java_ShoppingService_create(
      base::android::AttachCurrentThread(), reinterpret_cast<jlong>(this)));
  scoped_subscriptions_observer_.Observe(shopping_service_);
}

ShoppingServiceAndroid::~ShoppingServiceAndroid() {
  Java_ShoppingService_destroy(base::android::AttachCurrentThread(), java_ref_);
}

ShoppingService* ShoppingServiceAndroid::GetShoppingService() {
  return shopping_service_;
}

void ShoppingServiceAndroid::GetProductInfoForUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_gurl,
    const JavaParamRef<jobject>& j_callback) {
  CHECK(shopping_service_);

  GURL url = url::GURLAndroid::ToNativeGURL(env, j_gurl);

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

  GURL url = url::GURLAndroid::ToNativeGURL(env, j_gurl);

  std::optional<ProductInfo> info =
      shopping_service_->GetAvailableProductInfoForUrl(url);

  ScopedJavaLocalRef<jobject> info_java_object(nullptr);
  if (info.has_value()) {
    info_java_object = Java_ShoppingService_createProductInfo(
        env, ConvertUTF8ToJavaString(env, info->title),
        url::GURLAndroid::FromNativeGURL(env, GURL(info->image_url)),
        info->product_cluster_id.has_value(),
        info->product_cluster_id.value_or(0), info->offer_id.has_value(),
        info->offer_id.value_or(0),
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
    const std::optional<const ProductInfo>& info) {
  ScopedJavaLocalRef<jobject> info_java_object(nullptr);
  if (info.has_value()) {
    info_java_object = Java_ShoppingService_createProductInfo(
        env, ConvertUTF8ToJavaString(env, info->title),
        url::GURLAndroid::FromNativeGURL(env, GURL(info->image_url)),
        info->product_cluster_id.has_value(),
        info->product_cluster_id.value_or(0), info->offer_id.has_value(),
        info->offer_id.value_or(0),
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

  GURL url = url::GURLAndroid::ToNativeGURL(env, j_gurl);

  shopping_service_->GetMerchantInfoForUrl(
      url, base::BindOnce(&ShoppingServiceAndroid::HandleMerchantInfoCallback,
                          weak_ptr_factory_.GetWeakPtr(), env,
                          ScopedJavaGlobalRef<jobject>(j_callback)));
}

void ShoppingServiceAndroid::HandleMerchantInfoCallback(
    JNIEnv* env,
    const ScopedJavaGlobalRef<jobject>& callback,
    const GURL& url,
    std::optional<MerchantInfo> info) {
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

void ShoppingServiceAndroid::GetPriceInsightsInfoForUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_gurl,
    const JavaParamRef<jobject>& j_callback) {
  CHECK(shopping_service_);

  GURL url = url::GURLAndroid::ToNativeGURL(env, j_gurl);

  shopping_service_->GetPriceInsightsInfoForUrl(
      url,
      base::BindOnce(&ShoppingServiceAndroid::HandlePriceInsightsInfoCallback,
                     weak_ptr_factory_.GetWeakPtr(), env,
                     ScopedJavaGlobalRef<jobject>(j_callback)));
}

void ShoppingServiceAndroid::HandlePriceInsightsInfoCallback(
    JNIEnv* env,
    const ScopedJavaGlobalRef<jobject>& callback,
    const GURL& url,
    const std::optional<PriceInsightsInfo>& info) {
  ScopedJavaLocalRef<jobject> info_java_object(nullptr);
  if (info.has_value()) {
    ScopedJavaLocalRef<jobject> j_price_points = nullptr;
    for (const auto& point : info->catalog_history_prices) {
      j_price_points = Java_ShoppingService_createPricePointAndAddToList(
          env, j_price_points, ConvertUTF8ToJavaString(env, std::get<0>(point)),
          std::get<1>(point));
    }

    info_java_object = Java_ShoppingService_createPriceInsightsInfo(
        env, info->product_cluster_id.has_value(),
        info->product_cluster_id.value_or(0),
        ConvertUTF8ToJavaString(env, info->currency_code),
        info->typical_low_price_micros.has_value(),
        info->typical_low_price_micros.value_or(0),
        info->typical_high_price_micros.has_value(),
        info->typical_high_price_micros.value_or(0),
        info->catalog_attributes.has_value(),
        ConvertUTF8ToJavaString(env, info->catalog_attributes.value_or("")),
        j_price_points, info->jackpot_url.has_value(),
        url::GURLAndroid::FromNativeGURL(env,
                                         info->jackpot_url.value_or(GURL())),
        static_cast<int>(info->price_bucket), info->has_multiple_catalogs);
  }

  Java_ShoppingService_runPriceInsightsInfoCallback(
      env, callback, url::GURLAndroid::FromNativeGURL(env, url),
      info_java_object);
}

void ShoppingServiceAndroid::GetDiscountInfoForUrl(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_gurl,
    const JavaParamRef<jobject>& j_callback) {
  CHECK(shopping_service_);

  GURL url = url::GURLAndroid::ToNativeGURL(env, j_gurl);

  shopping_service_->GetDiscountInfoForUrl(
      {url}, base::BindOnce(&ShoppingServiceAndroid::HandleDiscountInfoCallback,
                            weak_ptr_factory_.GetWeakPtr(), env,
                            ScopedJavaGlobalRef<jobject>(j_callback)));
}

void ShoppingServiceAndroid::HandleDiscountInfoCallback(
    JNIEnv* env,
    const ScopedJavaGlobalRef<jobject>& callback,
    const GURL& url,
    const std::vector<DiscountInfo> info) {
  ScopedJavaLocalRef<jobjectArray> discount_info_array_obj =
      ConvertToJavaDiscountInfos(env, info);
  Java_ShoppingService_runDiscountInfoCallback(
      env, callback, url::GURLAndroid::FromNativeGURL(env, url),
      discount_info_array_obj);
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
    const JavaParamRef<jstring>& j_seen_locale,
    const JavaParamRef<jobject>& j_callback) {
  std::string id = ConvertJavaStringToUTF8(j_id);
  std::string seen_offer_id = ConvertJavaStringToUTF8(j_seen_offer_id);
  std::string seen_country = ConvertJavaStringToUTF8(j_seen_country);
  std::string seen_locale = ConvertJavaStringToUTF8(j_seen_locale);
  CHECK(!id.empty());

  auto user_seen_offer = std::make_optional<UserSeenOffer>(
      seen_offer_id, j_seen_price, seen_country, seen_locale);
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
                           kUnknownSubscriptionTimestamp, std::nullopt);
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
                           kUnknownSubscriptionTimestamp, std::nullopt);

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
                           kUnknownSubscriptionTimestamp, std::nullopt);

  return shopping_service_->IsSubscribedFromCache(std::move(sub));
}

void ShoppingServiceAndroid::GetAllPriceTrackedBookmarks(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_callback) {
  shopping_service_->GetAllPriceTrackedBookmarks(base::BindOnce(
      [](JNIEnv* env, const ScopedJavaGlobalRef<jobject>& callback,
         std::vector<const bookmarks::BookmarkNode*> tracked_items) {
        std::vector<int64_t> ids;
        for (const bookmarks::BookmarkNode* bookmark : tracked_items) {
          ids.push_back(bookmark->id());
        }
        Java_ShoppingService_runGetAllPriceTrackedBookmarksCallback(
            env, callback, base::android::ToJavaLongArray(env, ids));
      },
      env, ScopedJavaGlobalRef<jobject>(j_callback)));
}

void ShoppingServiceAndroid::OnSubscribe(const CommerceSubscription& sub,
                                         bool succeeded) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ShoppingService_onSubscribe(env, java_ref_, ToJavaObject(env, sub),
                                   succeeded);
}

void ShoppingServiceAndroid::OnUnsubscribe(const CommerceSubscription& sub,
                                           bool succeeded) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ShoppingService_onUnsubscribe(env, java_ref_, ToJavaObject(env, sub),
                                     succeeded);
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

bool ShoppingServiceAndroid::IsCommercePriceTrackingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CHECK(shopping_service_);

  return shopping_service_->IsCommercePriceTrackingEnabled();
}

bool ShoppingServiceAndroid::IsPriceInsightsEligible(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CHECK(shopping_service_);

  return shopping_service_->IsPriceInsightsEligible();
}

bool ShoppingServiceAndroid::IsDiscountEligibleToShowOnNavigation(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  CHECK(shopping_service_);

  return shopping_service_->IsDiscountEligibleToShowOnNavigation();
}

}  // namespace commerce
