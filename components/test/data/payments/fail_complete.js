/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */

/**
 * Launches the PaymentRequest UI and always fails to complete the transaction.
 *
 * Legacy entry function until basic-card is removed.
 */
function buy() { // eslint-disable-line no-unused-vars
  buyWithMethods(
      [{supportedMethods: 'basic-card', data: {supportedNetworks: ['visa']}}]);
}

/**
 * Launches the PaymentRequest UI and always fails to complete the transaction.
 *
 * @param {sequence<PaymentMethodData>} methodData - An array of payment method
 *        objects.
 */
function buyWithMethods(methodData) {
  try {
    new PaymentRequest(
        methodData,
        {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}})
        .show()
        .then(function(resp) {
          resp.complete('fail')
              .then(function() {
                print('Transaction failed');
              })
              .catch(function(error) {
                print(error.message);
              });
        })
        .catch(function(error) {
          print(error.message);
        });
  } catch (error) {
    print(error.message);
  }
}
