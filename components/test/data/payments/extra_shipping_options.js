/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI with shipping options, but does not request a
 * shipping address.
 * @param {String} methodData - An array of payment method objects.
 */
function buyWithMethods(methodData) {
  try {
    const details = {
      total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
      shippingOptions: [{
        id: 'freeShippingOption',
        label: 'Free global shipping',
        amount: {currency: 'USD', value: '0'},
        selected: true,
      }],
    };
    const request = new PaymentRequest(methodData, details);
    request.show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(
                    resp.methodName + '<br>' +
                    JSON.stringify(resp.details, undefined, 2));
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
