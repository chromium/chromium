// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/feature_utils.h"

#include "base/android/jni_android.h"
#include "components/commerce/core/android/shopping_service_android.h"
#include "components/commerce/core/shopping_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/commerce/core/android/core_jni/CommerceFeatureUtils_jni.h"

namespace commerce {

static bool JNI_CommerceFeatureUtils_IsShoppingListEligible(
    JNIEnv* env,
    int64_t shopping_service_android_ptr) {
  if (!shopping_service_android_ptr) {
    return false;
  }
  ShoppingService* service =
      reinterpret_cast<ShoppingServiceAndroid*>(shopping_service_android_ptr)
          ->GetShoppingService();
  return IsShoppingListEligible(service ? service->GetAccountChecker()
                                        : nullptr);
}

static bool JNI_CommerceFeatureUtils_IsDiscountInfoApiEnabled(
    JNIEnv* env,
    int64_t shopping_service_android_ptr) {
  if (!shopping_service_android_ptr) {
    return false;
  }
  ShoppingService* service =
      reinterpret_cast<ShoppingServiceAndroid*>(shopping_service_android_ptr)
          ->GetShoppingService();
  return IsDiscountInfoApiEnabled(service ? service->GetAccountChecker()
                                          : nullptr);
}

static bool JNI_CommerceFeatureUtils_IsPriceAnnotationsEnabled(
    JNIEnv* env,
    int64_t shopping_service_android_ptr) {
  if (!shopping_service_android_ptr) {
    return false;
  }
  ShoppingService* service =
      reinterpret_cast<ShoppingServiceAndroid*>(shopping_service_android_ptr)
          ->GetShoppingService();
  return IsPriceAnnotationsEnabled(service ? service->GetAccountChecker()
                                           : nullptr);
}

}  // namespace commerce

DEFINE_JNI(CommerceFeatureUtils)
