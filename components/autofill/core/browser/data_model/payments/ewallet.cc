// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/payments/ewallet.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/containers/to_vector.h"
#include "url/android/gurl_android.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/autofill/android/payments_jni_headers/Ewallet_jni.h"
#include "components/autofill/android/payments_jni_headers/PaymentInstrument_jni.h"
#endif

namespace autofill {

// TODO(crbug.com/40280186): Change the type of `supported_payment_link_uris` to
// `base::flat_set<std::string>`.
Ewallet::Ewallet(int64_t instrument_id,
                 std::u16string nickname,
                 GURL display_icon_url,
                 std::u16string ewallet_name,
                 std::u16string account_display_name,
                 base::flat_set<std::u16string> supported_payment_link_uris,
                 bool is_fido_enrolled)
    : ewallet_name_(std::move(ewallet_name)),
      account_display_name_(std::move(account_display_name)),
      supported_payment_link_uris_(std::move(supported_payment_link_uris)),
      payment_instrument_(
          instrument_id,
          nickname,
          display_icon_url,
          DenseSet({PaymentInstrument::PaymentRail::kPaymentHyperlink}),
          is_fido_enrolled) {}
Ewallet::Ewallet(const Ewallet& other) = default;
Ewallet& Ewallet::operator=(const Ewallet& other) = default;
Ewallet::~Ewallet() = default;

std::strong_ordering operator<=>(const Ewallet& a, const Ewallet& b) = default;
bool operator==(const Ewallet& a, const Ewallet& b) = default;

bool Ewallet::SupportsPaymentLink(std::string_view payment_link) const {
  return std::ranges::any_of(
      supported_payment_link_uris_,
      [&payment_link](const std::u16string& supported_uri) {
        return re2::RE2::FullMatch(payment_link,
                                   base::UTF16ToUTF8(supported_uri));
      });
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> CreateJavaEwalletFromNative(
    JNIEnv* env,
    const Ewallet& ewallet) {
  const DenseSet<PaymentInstrument::PaymentRail>&
      payment_instrument_supported_rails =
          ewallet.payment_instrument().supported_rails();
  std::vector<int32_t> supported_payment_rails_array =
      base::ToVector(payment_instrument_supported_rails,
                     [](PaymentInstrument::PaymentRail rail) {
                       return static_cast<int32_t>(rail);
                     });

  base::android::ScopedJavaLocalRef<jobject> jdisplay_icon_url = nullptr;
  if (!ewallet.payment_instrument().display_icon_url().is_empty()) {
    jdisplay_icon_url = url::GURLAndroid::FromNativeGURL(
        env, ewallet.payment_instrument().display_icon_url());
  }

  return Java_Ewallet_create(env, ewallet.payment_instrument().instrument_id(),
                             ewallet.payment_instrument().nickname(),
                             jdisplay_icon_url, supported_payment_rails_array,
                             ewallet.payment_instrument().is_fido_enrolled(),
                             ewallet.ewallet_name(),
                             ewallet.account_display_name());
}

Ewallet CreateNativeEwalletFromJava(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& jewallet) {
  int64_t instrument_id = Java_PaymentInstrument_getInstrumentId(env, jewallet);
  std::u16string nickname = Java_PaymentInstrument_getNickname(env, jewallet);
  const base::android::ScopedJavaLocalRef<jobject>& jdisplay_icon_url =
      Java_PaymentInstrument_getDisplayIconUrl(env, jewallet);
  GURL display_icon_url;
  if (!jdisplay_icon_url.is_null()) {
    display_icon_url = url::GURLAndroid::ToNativeGURL(env, jdisplay_icon_url);
  }
  bool is_fido_enrolled =
      Java_PaymentInstrument_getIsFidoEnrolled(env, jewallet);
  std::u16string ewallet_name = Java_Ewallet_getEwalletName(env, jewallet);
  std::u16string account_display_name =
      Java_Ewallet_getAccountDisplayName(env, jewallet);
  return Ewallet(instrument_id, nickname, display_icon_url, ewallet_name,
                 account_display_name, /*supported_payment_link_uris=*/{},
                 is_fido_enrolled);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace autofill
