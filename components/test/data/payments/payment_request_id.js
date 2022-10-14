/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI including a details.id and returns the
 * request identifier from the response.
 * @param {string} method - The payment method identifier to use.
 * @return {string} - The request identifier from the response.
 */
async function getResponseId(method) {
  try {
    const request = new PaymentRequest(
        [{supportedMethods: method}],
        {
          id: 'my_payment_id',
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
        });
    const response = await request.show();
    await response.complete('success');
    return response.requestId;
  } catch (error) {
    return error.toString();
  }
}
