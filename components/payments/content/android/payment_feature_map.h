// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_FEATURE_MAP_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_FEATURE_MAP_H_

#include "base/feature_list.h"

namespace payments {
namespace android {

// Android only payment features in alphabetical order:

// If enabled, remove the deprecated parameters from the intents that are sent
// to Android payment apps.
BASE_DECLARE_FEATURE(kAndroidPaymentIntentsOmitDeprecatedParameters);

// If enabled, use Android intents for the following payment methods:
// - https://google.com/pay
// - https://pay.google.com/authentication.
BASE_DECLARE_FEATURE(kGooglePayViaAndroidIntents);

// If enabled, then the web merchant origin and web wallet parameters will be
// omitted from the isReadyToPayRequest. See: https://crbug.com/1406655.
BASE_DECLARE_FEATURE(kOmitParametersInReadyToPay);

// If enabled, then apps will be available to show even if the "is ready to pay"
// query returns false. See: crbug.com/427918761.
BASE_DECLARE_FEATURE(kAllowShowWithoutReadyToPay);

// If enabled, the payment details update service reconnects to the Android
// payment app when the connection is lost unexpectedly.
BASE_DECLARE_FEATURE(kReconnectOnLostConnectionToUpdatePaymentDetailsService);

// If enabled, then Clank displays an alert dialog with the content of the
// IS_READY_TO_PAY intent, whenever Clank fires this intent.
BASE_DECLARE_FEATURE(kShowReadyToPayDebugInfo);

// If enabled, PaymentRequest looks up the UPDATE_PAYMENT_DETAILS intent filter
// in the payment app.
BASE_DECLARE_FEATURE(kUpdatePaymentDetailsIntentFilterInPaymentApp);

}  // namespace android
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_FEATURE_MAP_H_
