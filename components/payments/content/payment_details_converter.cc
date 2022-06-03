// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_details_converter.h"

#include <utility>

#include "base/callback.h"
#include "base/check.h"

namespace payments {

// Keep in sync with PaymentDetailsConverter.java.

mojom::PaymentRequestDetailsUpdatePtr
PaymentDetailsConverter::ConvertToPaymentRequestDetailsUpdate(
    const mojom::PaymentDetailsPtr& details,
    bool handles_shipping,
    const MethodChecker& method_checker) {
  DCHECK(details);

  auto response = mojom::PaymentRequestDetailsUpdate::New();
  response->error = details->error;
  response->stringified_payment_method_errors =
      details->stringified_payment_method_errors;
  if (handles_shipping && details->shipping_address_errors) {
    response->shipping_address_errors =
        details->shipping_address_errors.Clone();
  }

  if (details->total)
    response->total = details->total->amount.Clone();

  if (details->modifiers) {
    response->modifiers = std::vector<mojom::PaymentHandlerModifierPtr>();

    for (const auto& merchant : *details->modifiers) {
      bool is_valid = false;
      method_checker.Run(merchant->method_data->supported_method, &is_valid);
      if (!is_valid)
        continue;

      mojom::PaymentHandlerModifierPtr mod =
          mojom::PaymentHandlerModifier::New();
      mod->method_data = mojom::PaymentHandlerMethodData::New();
      mod->method_data->method_name = merchant->method_data->supported_method;
      mod->method_data->stringified_data =
          merchant->method_data->stringified_data;

      if (merchant->total)
        mod->total = merchant->total->amount.Clone();

      response->modifiers->emplace_back(std::move(mod));
    }
  }

  if (handles_shipping && details->shipping_options) {
    response->shipping_options = std::vector<mojom::PaymentShippingOptionPtr>();
    for (const auto& option : *details->shipping_options)
      response->shipping_options->emplace_back(option.Clone());
  }

  return response;
}

}  // namespace payments
