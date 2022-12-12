/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI with Bob Pay, Alice Pay as payment methods.
 */
function buy() {
  try {
    new PaymentRequest(
        [
          {supportedMethods: 'https://alicepay.test'},
          {supportedMethods: 'https://bobpay.test'},
          {supportedMethods: 'https://charliepay.test'},
        ],
        {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}})
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(
                    resp.methodName + '<br>' +
                    JSON.stringify(resp.details, undefined, 2));
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
