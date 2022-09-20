// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/mock_payment_app_factory_delegate.h"

namespace payments {

MockPaymentAppFactoryDelegate::MockPaymentAppFactoryDelegate(
    content::WebContents* web_contents,
    mojom::PaymentMethodDataPtr method_data)
    : web_contents_(web_contents),
      top_origin_("https://top-origin.test"),
      frame_origin_("https://frame-origin.test") {
  SetRequestedPaymentMethod(std::move(method_data));
}

MockPaymentAppFactoryDelegate::~MockPaymentAppFactoryDelegate() = default;

void MockPaymentAppFactoryDelegate::SetRequestedPaymentMethod(
    mojom::PaymentMethodDataPtr method_data) {
  auto details = mojom::PaymentDetails::New();
  details->id = "id";

  std::vector<mojom::PaymentMethodDataPtr> methods;
  methods.emplace_back(std::move(method_data));

  spec_ = std::make_unique<PaymentRequestSpec>(
      mojom::PaymentOptions::New(), std::move(details), std::move(methods),
      /*observer=*/nullptr, /*app_locale=*/"en-US");
}

}  // namespace payments
