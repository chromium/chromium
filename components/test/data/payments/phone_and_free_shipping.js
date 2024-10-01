/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI that request a phone number and offers free
 * shipping worldwide.
 *
 * @param {Array<Object>} methods An array of payment method objects.
 */
function buyWithMethods(methods) {
  try {
    const request = new PaymentRequest(
        methods, {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          shippingOptions: [{
            id: 'freeShippingOption',
            label: 'Free global shipping',
            amount: {currency: 'USD', value: '0'},
            selected: true,
          }],
        },
        {requestPayerPhone: true, requestShipping: true});
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
