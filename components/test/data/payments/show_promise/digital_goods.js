/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

let request = null;

/**
 * Create an instance of PaymentRequest.
 * @param {DOMString} supportedMethods - The payment method name.
 */
function create(supportedMethods) {
  if (!supportedMethods) {
    print('supportedMethods required');
    return;
  }
  try {
    request = new PaymentRequest([{supportedMethods}], {
      total:
          {label: 'PENDING TOTAL', amount: {currency: 'USD', value: '99.99'}},
    });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Launch PaymentRequest with a show promise for digital goods.
 */
function buy() {
  try {
    request
        .show(new Promise(function(resolve) {
          resolve({
            total: {label: 'Total', amount: {currency: 'USD', value: '1.00'}},
          });
        }))
        .then(function(result) {
          print(JSON.stringify(result.details));
          return result.complete('success');
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error);
  }
}
