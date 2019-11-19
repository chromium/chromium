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
  try {
    new PaymentRequest(
        [{
          supportedMethods: 'basic-card',
          data: {supportedNetworks: ['visa', 'mastercard']},
        }],
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
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error.message);
  }
}
