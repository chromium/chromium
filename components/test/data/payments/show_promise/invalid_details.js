/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launch PaymentRequest with a show promise that resolve with invalid details.
 * @param {string} supportedMethods The payment method that is supported by this
 *        request.
 * @return {string} - The error message, if any.
 */
async function buyWithMethods(supportedMethods) {
  try {
    await new PaymentRequest([{supportedMethods}], {
      total: {
        label: 'PENDING TOTAL',
        amount: {currency: 'USD', value: '99.99'},
      },
    })
        .show(new Promise(function(resolve) {
          resolve({
            total: {
              label: 'Total',
              amount: {currency: 'USD', value: '-1.00'},
            },
          });
        }));
  } catch (error) {
    // Error is both printed and returned as the Java test reads it from the
    // page and the C++ browser test reads the return value.
    print(error);
    return error.toString();
  }
}
