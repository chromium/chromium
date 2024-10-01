/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI that prints the shipping address received
 * on shippingAddressChange events at the end of the transaction.
 */
function buy() {
  try {
    const details = {
      total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
      displayItems: [
        {
          label: 'Pending shipping price',
          amount: {currency: 'USD', value: '0.00'},
          pending: true,
        },
        {label: 'Subtotal', amount: {currency: 'USD', value: '5.00'}},
      ],
    };

    const request = new PaymentRequest(
        [{supportedMethods: 'basic-card', data: {supportedNetworks: ['visa']}}],
        details, {requestShipping: true});

    request.addEventListener('shippingaddresschange', function(evt) {
      evt.updateWith(new Promise(function(resolve) {
        print(JSON.stringify(request.shippingAddress, undefined, 2));
        resolve(details);
      }));
    });

    request.show();
  } catch (error) {
    print(error.message);
  }
}
