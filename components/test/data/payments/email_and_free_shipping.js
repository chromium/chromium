/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI that requests an email address and offers free
 * shipping worldwide.
 *
 * Legacy entry function until basic-card is removed.
 */
function buy() {
  buyWithMethods(
      [{supportedMethods: 'basic-card', data: {supportedNetworks: ['visa']}}]);
}

/**
 * Launches the PaymentRequest UI that requests an email address and offers free
 * shipping worldwide.
 *
 * @param {sequence<PaymentMethodData>} methodData - An array of payment method
 *        objects.
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
    const request = new PaymentRequest(
        methodData, details, {requestPayerEmail: true, requestShipping: true});
    request.addEventListener('shippingaddresschange', function(e) {
      e.updateWith(details);
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
