// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PAYMENT_INFORMATION_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PAYMENT_INFORMATION_H_

#include <string>

namespace autofill_assistant {

// Struct for holding the payment information data.
struct PaymentInformation {
  PaymentInformation();
  ~PaymentInformation();

  bool succeed;
  std::string card_guid;
  std::string card_issuer_network;
  std::string address_guid;
  std::string payer_name;
  std::string payer_phone;
  std::string payer_email;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_PAYMENT_INFORMATION_H_
