/*
 * Copyright 2022 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/** {object} - The details of the payment response. */
let details = {};

/**
 * Invokes PaymentRequest and saves the response details in `details`.
 * @param {string} method - The payment method name.
 * @return {string} - The string "success" or an error message.
 */
async function invokePaymentRequest(method) {
  const total = {label: 'Total', amount: {currency: 'USD', value: '0.01'}};
  try {
    const request = new PaymentRequest(
        [{supportedMethods: method}],
        {total, modifiers: [{supportedMethods: method, total}]});
    const response = await request.show();
    await response.complete('success');
    details = response.details;
    return 'success';
  } catch (error) {
    return error.message;
  }
}
