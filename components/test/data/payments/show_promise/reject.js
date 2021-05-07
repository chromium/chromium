/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launch PaymentRequest with a show promise and reject that promise.
 * @param {boolean} useUrlPaymentMethod - Whether URL payment method should be
 * used. Useful for payment handlers, which cannot use basic-card payment
 * method. By default, basic-card payment method is used.
 */
function buy(useUrlPaymentMethod) { // eslint-disable-line no-unused-vars
  try {
    let supportedMethods = 'basic-card';
    if (useUrlPaymentMethod) {
      supportedMethods = window.location.href;
    }
    new PaymentRequest(
        [{supportedMethods}],
        {total: {label: 'Total', amount: {currency: 'USD', value: '1.00'}}})
        .show(new Promise(function(resolve, reject) {
          reject();
        }))
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error);
  }
}
