/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI that requests email address.
 */
function buy() {
  buyWithMethods([
    {supportedMethods: 'https://bobpay.test'},
  ]);
}

/**
 * Launches the PaymentRequest UI that requests email address.
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
function buyWithMethods(methodData) {
  try {
    new PaymentRequest(
        methodData,
        {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}},
        {requestPayerEmail: true})
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(
                    resp.payerEmail + '<br>' + resp.methodName + '<br>' +
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
