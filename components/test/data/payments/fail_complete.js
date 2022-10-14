/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI and always fails to complete the transaction.
 *
 * @param {sequence<PaymentMethodData>} methodData - An array of payment method
 *        objects.
 */
function buyWithMethods(methodData) {
  failComplete(methodData)
      .then((result) => print(result))
      .catch((error) => print(error.message));
}

/**
 * Launches the PaymentRequest UI and always fails to complete the transaction,
 * returning 'Transaction failed' if there was no error.
 *
 * @param {sequence<PaymentMethodData>} methodData - An array of payment method
 *        objects.
 * @return {string} - 'Transaction failed' if there was no error, otherwise the
 *        error message.
 */
async function failComplete(methodData) {
  try {
    const request = new PaymentRequest(
        methodData,
        {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}});
    const response = await request.show();
    await response.complete('fail');
    return 'Transaction failed';
  } catch (error) {
    return error.toString();
  }
}
