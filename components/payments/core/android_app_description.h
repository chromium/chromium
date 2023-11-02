// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_ANDROID_APP_DESCRIPTION_H_
#define COMPONENTS_PAYMENTS_CORE_ANDROID_APP_DESCRIPTION_H_

#include <memory>
#include <string>
#include <vector>

namespace payments {

// Describes an Android activity with org.chromium.intent.action.PAY intent
// filter. Documentation:
// https://web.dev/android-payment-apps-overview/
struct AndroidActivityDescription {
  AndroidActivityDescription();
  ~AndroidActivityDescription();

  // Disallow copy and assign.
  AndroidActivityDescription(const AndroidActivityDescription& other) = delete;
  AndroidActivityDescription& operator=(
      const AndroidActivityDescription& other) = delete;

  // The name of the activity, e.g., "com.example.app.PaymentActivity".
  std::string name;

  // The payment method identifier from the
  // "org.chromium.default_payment_method_name" metadata value of this activity.
  // For example, "https://example.com/web-pay".
  //
  // The metadata value of "org.chromium.payment_method_names" is not yet used
  // here, so it's omitted from the struct at this time.
  std::string default_payment_method;
};

// Describes an Android app that can handle payments.
struct AndroidAppDescription {
  AndroidAppDescription();
  ~AndroidAppDescription();

  // Disallow copy and assign.
  AndroidAppDescription(const AndroidAppDescription& other) = delete;
  AndroidAppDescription& operator=(const AndroidAppDescription& other) = delete;

  // The name of the Android package of this app, e.g., "com.example.app".
  std::string package;

  // The list of activities with org.chromium.intent.action.PAY intent filters
  // in this app.
  std::vector<std::unique_ptr<AndroidActivityDescription>> activities;

  // The list of service names with org.chromium.intent.action.IS_READY_TO_PAY
  // intent filters in this app. For example,
  // ["com.example.app.IsReadyToPayService"].
  //
  // Note that it's a mistake to declare multiple IS_READY_TO_PAY services in an
  // app. This mistake would be reported to the developer.
  std::vector<std::string> service_names;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_ANDROID_APP_DESCRIPTION_H_
