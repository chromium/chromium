// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_REQUEST_UPDATE_EVENT_LISTENER_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_REQUEST_UPDATE_EVENT_LISTENER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/payments/content/payment_handler_host.h"

namespace payments {
namespace android {

class PaymentRequestUpdateEventListener
    : public payments::PaymentHandlerHost::Delegate {
 public:
  explicit PaymentRequestUpdateEventListener(
      const base::android::JavaParamRef<jobject>& listener);
  ~PaymentRequestUpdateEventListener() override;

  // PaymentHandlerHost::Delegate implementation:
  bool ChangePaymentMethod(const std::string& method_name,
                           const std::string& stringified_data) override;
  bool ChangeShippingOption(const std::string& shipping_option_id) override;
  bool ChangeShippingAddress(
      mojom::PaymentAddressPtr shipping_address) override;

  base::WeakPtr<PaymentRequestUpdateEventListener> AsWeakPtr();

 private:
  base::android::ScopedJavaGlobalRef<jobject> listener_;

  base::WeakPtrFactory<PaymentRequestUpdateEventListener> weak_ptr_factory_{
      this};
};

}  // namespace android
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_REQUEST_UPDATE_EVENT_LISTENER_H_
