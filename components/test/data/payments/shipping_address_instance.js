/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches PaymentRequest to check whether PaymentRequest.shippingAddress is
 * the same instance as PaymentResponse.shippingAddress.
 *
 * Legacy entry function until basic-card is removed.
 */
function buy() {
  buyWithMethods([{supportedMethods: 'basic-card'}]);
}

/**
 * Launches PaymentRequest to check whether PaymentRequest.shippingAddress is
 * the same instance as PaymentResponse.shippingAddress.
 *
 * @param {String} methodData - An array of payment method objects.
 */
function buyWithMethods(methodData) {
  try {
    const details = {
      total: {
        label: 'Total',
        amount: {
          currency: 'USD',
          value: '5.00',
        },
      },
      shippingOptions: [{
        id: 'freeShippingOption',
        label: 'Free global shipping',
        amount: {
          currency: 'USD',
          value: '0',
        },
        selected: true,
      }],
    };
    const request = new PaymentRequest(methodData, details, {
      requestShipping: true,
    });
    request.show()
        .then(function(resp) {
          print(
              'Same instance: ' +
              (request.shippingAddress === resp.shippingAddress).toString());
          resp.complete('success');
        })
        .catch(function(error) {
          print('User did not authorized transaction: ' + error);
        });
  } catch (error) {
    print('Developer mistake ' + error);
  }
}
