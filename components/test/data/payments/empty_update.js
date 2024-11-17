/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Invokes PaymentRequest with shipping and immediately rejects all shipping
 * addresses by calling updateWith({}), which is an "empty update."
 */
function buy() {
  buyWithMethods([{supportedMethods: 'basic-card'}]);
}

/**
 * Invokes PaymentRequest with shipping and immediately rejects all shipping
 * addresses by calling updateWith({}), which is an "empty update."
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
function buyWithMethods(methodData) {
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

    const request =
        new PaymentRequest(methodData, details, {requestShipping: true});

    request.addEventListener('shippingaddresschange', function(evt) {
      evt.updateWith({});
    });

    request.show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print(error);
              });
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error.message);
  }
}
