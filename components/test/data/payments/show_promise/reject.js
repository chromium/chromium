/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launch PaymentRequest with a show promise and reject that promise.
 * @param {boolean} useUrlPaymentMethod - Whether URL payment method should be
 * used. Useful for payment handlers, which cannot use basic-card payment
 * method. By default, basic-card payment method is used.
 * @return {string} - The error message, if any.
 */
async function buy(useUrlPaymentMethod) {
  try {
    let supportedMethods = 'basic-card';
    if (useUrlPaymentMethod) {
      supportedMethods = window.location.href;
    }
    await new PaymentRequest(
        [{supportedMethods}],
        {total: {label: 'Total', amount: {currency: 'USD', value: '1.00'}}})
        .show(new Promise(function(resolve, reject) {
          reject('rejected');
        }));
  } catch (error) {
    // Error is both printed and returned as the Java test reads it from the
    // page and the C++ browser test reads the return value.
    print(error);
    return error.toString();
  }
}
