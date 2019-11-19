/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

var request = null;

/**
 * Create an instance of PaymentRequest.
 */
function create() { // eslint-disable-line no-unused-vars
  try {
    request = new PaymentRequest([{supportedMethods: 'basic-card'}], {
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
function buy() { // eslint-disable-line no-unused-vars
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
