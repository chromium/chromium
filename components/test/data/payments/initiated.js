/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Checks whether the payment handler can make payments.
 * @param {string} method - The payment method name to check.
 * @return {Promise<bool|string>} - true, false, or error message on failure.
 */
async function canMakePayment(method) {
  try {
    return new PaymentRequest([{supportedMethods: method}], {
      total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}},
    }).canMakePayment();
  } catch (e) {
    return e.toString();
  }
}
