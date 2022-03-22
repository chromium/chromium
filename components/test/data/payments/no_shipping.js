/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */

/**
 * Launches the PaymentRequest UI that does not require a shipping address.
 */
function buy() { // eslint-disable-line no-unused-vars
  buyWithMethods([
    {
      supportedMethods: 'basic-card',
      data: {supportedNetworks: ['visa', 'mastercard']},
    },
  ]);
}

/**
 * Launches the PaymentRequest UI that does not require a shipping address.
 * @param {String} methodData - An array of payment method objects.
 * @return {string} - The error message, if any.
 */
async function buyWithMethods(methodData) {
  try {
    await new PaymentRequest(
      methodData,
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          displayItems: [
            {
              label: 'Subtotal',
              amount: {currency: 'USD', value: '4.50'},
              pending: true,
            },
            {label: 'Taxes', amount: {currency: 'USD', value: '0.50'}},
          ],
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print(error);
              });
        });
  } catch (error) {
    return error.message;
  }
}
