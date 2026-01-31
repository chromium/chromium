// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_VALUABLES_ANDROID_LOYALTY_CARD_ANDROID_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_VALUABLES_ANDROID_LOYALTY_CARD_ANDROID_H_

#include <cstdint>

#include "base/component_export.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/data_model/valuables/valuable_types.h"
#include "third_party/jni_zero/jni_zero.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/autofill/android/main_autofill_jni_headers/LoyaltyCard_jni.h"

namespace jni_zero {

template <>
inline autofill::LoyaltyCard FromJniType<autofill::LoyaltyCard>(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_object) {
  autofill::ValuableId loyalty_card_id = autofill::ValuableId(
      autofill::Java_LoyaltyCard_getLoyaltyCardId(env, j_object));
  std::string merchant_name =
      autofill::Java_LoyaltyCard_getMerchantName(env, j_object);
  std::string program_name =
      autofill::Java_LoyaltyCard_getProgramName(env, j_object);
  GURL program_logo = autofill::Java_LoyaltyCard_getProgramLogo(env, j_object);
  std::string loyalty_card_number =
      autofill::Java_LoyaltyCard_getLoyaltyCardNumber(env, j_object);
  std::vector<GURL> merchant_domains =
      autofill::Java_LoyaltyCard_getMerchantDomains(env, j_object);
  base::Time use_date = base::Time::FromMillisecondsSinceUnixEpoch(
      autofill::Java_LoyaltyCard_getUseDate(env, j_object));
  int64_t use_count = autofill::Java_LoyaltyCard_getUseCount(env, j_object);
  return autofill::LoyaltyCard(
      std::move(loyalty_card_id), std::move(merchant_name),
      std::move(program_name), std::move(program_logo),
      std::move(loyalty_card_number), std::move(merchant_domains), use_date,
      use_count);
}

template <>
inline jni_zero::ScopedJavaLocalRef<jobject> ToJniType<autofill::LoyaltyCard>(
    JNIEnv* env,
    const autofill::LoyaltyCard& loyalty_card) {
  return autofill::Java_LoyaltyCard_Constructor(
      env, *loyalty_card.id(), loyalty_card.merchant_name(),
      loyalty_card.program_name(), loyalty_card.program_logo(),
      loyalty_card.loyalty_card_number(), loyalty_card.merchant_domains(),
      loyalty_card.use_date().InMillisecondsSinceUnixEpoch(),
      loyalty_card.use_count());
}

}  // namespace jni_zero

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MODEL_VALUABLES_ANDROID_LOYALTY_CARD_ANDROID_H_

DEFINE_JNI(LoyaltyCard)
