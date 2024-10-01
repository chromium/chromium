/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI that does not require a shipping address.
 * @param {String} methodData - An array of payment method objects.
 * @return {string} - The error message, if any.
 */
async function buyWithMethods(methodData) {
  try {
    await new PaymentRequest(
      methodData,
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          displayItems: [
            {
              label: 'Subtotal',
              amount: {currency: 'USD', value: '4.50'},
              pending: true,
            },
            {label: 'Taxes', amount: {currency: 'USD', value: '0.50'}},
          ],
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print(error);
              });
        });
  } catch (error) {
    return error.message;
  }
}

// TODO: Migrate tests using no_shipping.js to triggerPaymentRequest/getResult.
let gShowPromise = null;

/**
 * Launches the PaymentRequest UI that does not require a shipping address.
 *
 * @param {!Array<!Object>} methodData: Payment methods data for PaymentRequest
 *     constructor.
 */
function triggerPaymentRequest(methodData) {
  const request = new PaymentRequest(methodData, {
    total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
    displayItems: [
      {
        label: 'Subtotal',
        amount: {currency: 'USD', value: '4.50'},
        pending: true,
      },
      {label: 'Taxes', amount: {currency: 'USD', value: '0.50'}},
    ],
  });
  gShowPromise = request.show();
}

/**
 * Waits for the outstanding gShowPromise to resolve, and returns either the
 * response or any error it generated.
 */
async function getResult() {
  try {
    const response = await gShowPromise;
    return await response.complete('success');
  } catch (e) {
    return e.message;
  }
}
