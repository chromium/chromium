/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

let request = null;
let showPromise = null;

const TEST_DETAILS = {
  total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
};

/**
 * Helper function that launches the PaymentRequest UI with the specified
 * payment methods.
 *
 * @param {!Array<!Object>} methods: Payment methods data for PaymentRequest
 *     constructor.
 */
function testPaymentMethods(methods) {
  request = new PaymentRequest(methods, TEST_DETAILS);
  showPromise = request.show();
}

/**
 * Launches the PaymentRequest UI with basic-card as payment methods.
 * Saves the newly created PaymentRequest and its show promise in global
 * variables. Tests can optionally call abort() to cancel this request.
 */
function testBasicCard() {
  testPaymentMethods([
    {supportedMethods: 'basic-card'},
  ]);
}

/**
 * Aborts the PaymentRequest initiated by testBasicCard().
 */
async function abort() {
  await request.abort();
  return await showPromise.catch((e) => {
    return e.name == 'AbortError';
  });
}

/**
 * Launches a PaymentRequest with https://google.com/pay payment method. The
 * test should use the fake Google Pay app at
 * //components/test/data/payments/google.com.
 * This function blocks until a response is received from the payment app.
 */
async function testGPay() {
  const gpayData = {
    supportedMethods: 'https://google.com/pay',
    data: {
      apiVersion: 1,
      allowedPaymentMethods: [{
        type: 'CARD',
      }],
    },
  };

  const request = new PaymentRequest([gpayData], TEST_DETAILS);
  const result = await request.show()
                     .then((response) => {
                       response.complete();
                       return JSON.stringify(response.details);
                     })
                     .catch((error) => {
                       return 'showPromise error: ' + error.message;
                     });
  return result;
}
