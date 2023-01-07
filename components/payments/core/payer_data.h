// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYER_DATA_H_
#define COMPONENTS_PAYMENTS_CORE_PAYER_DATA_H_

#include <string>

#include "components/payments/mojom/payment_request_data.mojom.h"

namespace payments {

struct PayerData {
  PayerData();
  PayerData(const std::string& payer_name,
            const std::string& payer_email,
            const std::string& payer_phone,
            mojom::PaymentAddressPtr shipping_address,
            const std::string& selected_shipping_option_id);
  ~PayerData();

  std::string payer_name;
  std::string payer_email;
  std::string payer_phone;
  mojom::PaymentAddressPtr shipping_address;
  std::string selected_shipping_option_id;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYER_DATA_H_
