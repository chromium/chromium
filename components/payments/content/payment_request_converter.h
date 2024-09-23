// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_CONVERTER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_CONVERTER_H_

#include "components/autofill/core/browser/data_model/credit_card.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

// TODO(crbug.com/41342247): Write unit tests for these functions.

namespace payments {

class PaymentDetails;
class PaymentMethodData;

// Returns the card network name associated with a given BasicCardNetwork. Names
// are inspired by https://www.w3.org/Payments/card-network-ids.
std::string GetBasicCardNetworkName(const mojom::BasicCardNetwork& network);

PaymentMethodData ConvertPaymentMethodData(
    const mojom::PaymentMethodDataPtr& method_data_entry);

PaymentDetails ConvertPaymentDetails(
    const mojom::PaymentDetailsPtr& details_entry);

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_CONVERTER_H_
