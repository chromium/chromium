// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payer_data.h"

namespace payments {

PayerData::PayerData() = default;
PayerData::PayerData(const std::string& payer_name,
                     const std::string& payer_email,
                     const std::string& payer_phone,
                     mojom::PaymentAddressPtr shipping_address,
                     const std::string& selected_shipping_option_id)
    : payer_name(payer_name),
      payer_email(payer_email),
      payer_phone(payer_phone),
      shipping_address(std::move(shipping_address)),
      selected_shipping_option_id(selected_shipping_option_id) {}
PayerData::~PayerData() = default;

}  // namespace payments