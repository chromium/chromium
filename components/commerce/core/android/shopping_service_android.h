// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COMMERCE_CORE_ANDROID_SHOPPING_SERVICE_ANDROID_H_
#define COMPONENTS_COMMERCE_CORE_ANDROID_SHOPPING_SERVICE_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "components/commerce/core/shopping_service.h"
#include "components/commerce/core/subscriptions/subscriptions_observer.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

class GURL;

namespace commerce {

class ShoppingService;
struct CommerceSubscription;

class ShoppingServiceAndroid : public base::SupportsUserData::Data,
                               public SubscriptionsObserver {
 public:
  ShoppingServiceAndroid(const ShoppingServiceAndroid&) = delete;
  ShoppingServiceAndroid& operator=(const ShoppingServiceAndroid&) = delete;

  ShoppingServiceAndroid(ShoppingService* service);
  ~ShoppingServiceAndroid() override;

  ShoppingService* GetShoppingService();

  void GetProductInfoForUrl(JNIEnv* env,
                            const JavaParamRef<jobject>& obj,
                            const JavaParamRef<jobject>& j_gurl,
                            const JavaParamRef<jobject>& j_callback);

  ScopedJavaLocalRef<jobject> GetAvailableProductInfoForUrl(
      JNIEnv* env,
      const JavaParamRef<jobject>& obj,
      const JavaParamRef<jobject>& j_gurl);

  void GetMerchantInfoForUrl(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             const JavaParamRef<jobject>& j_gurl,
                             const JavaParamRef<jobject>& j_callback);

  void GetPriceInsightsInfoForUrl(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  const JavaParamRef<jobject>& j_gurl,
                                  const JavaParamRef<jobject>& j_callback);

  void GetDiscountInfoForUrl(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             const JavaParamRef<jobject>& j_gurl,
                             const JavaParamRef<jobject>& j_callback);

  void FetchPriceEmailPref(JNIEnv* env, const JavaParamRef<jobject>& obj);

  void ScheduleSavedProductUpdate(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj);

  void Subscribe(JNIEnv* env,
                 const JavaParamRef<jobject>& obj,
                 jint j_type,
                 jint j_id_type,
                 jint j_management_type,
                 const JavaParamRef<jstring>& j_id,
                 const JavaParamRef<jstring>& j_seen_offer_id,
                 jlong j_seen_price,
                 const JavaParamRef<jstring>& j_seen_country,
                 const JavaParamRef<jstring>& j_seen_locale,
                 const JavaParamRef<jobject>& j_callback);

  void Unsubscribe(JNIEnv* env,
                   const JavaParamRef<jobject>& obj,
                   jint j_type,
                   jint j_id_type,
                   jint j_management_type,
                   const JavaParamRef<jstring>& j_id,
                   const JavaParamRef<jobject>& j_callback);

  void IsSubscribed(JNIEnv* env,
                    const JavaParamRef<jobject>& obj,
                    jint j_type,
                    jint j_id_type,
                    jint j_management_type,
                    const JavaParamRef<jstring>& j_id,
                    const JavaParamRef<jobject>& j_callback);

  bool IsSubscribedFromCache(JNIEnv* env,
                             const JavaParamRef<jobject>& obj,
                             jint j_type,
                             jint j_id_type,
                             jint j_management_type,
                             const JavaParamRef<jstring>& j_id);

  void GetAllPriceTrackedBookmarks(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   const JavaParamRef<jobject>& j_callback);

  bool IsShoppingListEligible(JNIEnv* env, const JavaParamRef<jobject>& obj);

  bool IsMerchantViewerEnabled(JNIEnv* env, const JavaParamRef<jobject>& obj);

  bool IsCommercePriceTrackingEnabled(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj);

  bool IsPriceInsightsEligible(JNIEnv* env, const JavaParamRef<jobject>& obj);

  bool IsDiscountEligibleToShowOnNavigation(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj);

  ScopedJavaGlobalRef<jobject> java_ref() { return java_ref_; }

 private:
  void HandleProductInfoCallback(JNIEnv* env,
                                 const ScopedJavaGlobalRef<jobject>& callback,
                                 const GURL& url,
                                 const std::optional<const ProductInfo>& info);

  void HandleMerchantInfoCallback(JNIEnv* env,
                                  const ScopedJavaGlobalRef<jobject>& callback,
                                  const GURL& url,
                                  std::optional<MerchantInfo> info);

  void HandlePriceInsightsInfoCallback(
      JNIEnv* env,
      const ScopedJavaGlobalRef<jobject>& callback,
      const GURL& url,
      const std::optional<PriceInsightsInfo>& info);

  void HandleDiscountInfoCallback(JNIEnv* env,
                                  const ScopedJavaGlobalRef<jobject>& callback,
                                  const GURL& url,
                                  const std::vector<DiscountInfo> info);

  void OnSubscribe(const CommerceSubscription& sub, bool succeeded) override;
  void OnUnsubscribe(const CommerceSubscription& sub, bool succeeded) override;

  // A handle to the backing shopping service. This is held as a raw pointer
  // since this object's lifecycle is tied to the service itself. This object
  // will always be destroyed before the service is.
  raw_ptr<ShoppingService> shopping_service_;

  // A handle to the java side of this object.
  ScopedJavaGlobalRef<jobject> java_ref_;

  base::ScopedObservation<ShoppingService, SubscriptionsObserver>
      scoped_subscriptions_observer_{this};

  base::WeakPtrFactory<ShoppingServiceAndroid> weak_ptr_factory_;
};

}  // namespace commerce

#endif  // COMPONENTS_COMMERCE_CORE_ANDROID_SHOPPING_SERVICE_ANDROID_H_
