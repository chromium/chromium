/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

let request = null;
let showPromise = null;

/**
 * Helper function that launches the PaymentRequest UI with the specified
 * payment methods.
 *
 * @param {!Array<!Object>} methods: Payment methods data for PaymentRequest
 *     constructor.
 * @param {boolean} requestShippingContact: Whether or not shipping address and
 *     payer's contact information are required.
 */
async function testPaymentMethods(methods, requestShippingContact = false) {
  const shippingOptions = requestShippingContact
      ? [{
          id: 'freeShippingOption',
          label: 'Free global shipping',
          amount: {currency: 'USD', value: '0'},
          selected: true,
        }]
      : [];
  try {
    request = new PaymentRequest(
        methods,
        {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
            shippingOptions},
        {
          requestShipping: requestShippingContact,
          requestPayerEmail: requestShippingContact,
          requestPayerName: requestShippingContact,
          requestPayerPhone: requestShippingContact,
        });
    showPromise = request.show();
    const resp = await showPromise;
    await resp.complete('success');
    const json = JSON.stringify(resp.details, undefined, 2);
    print(`${resp.methodName}<br>${json}`);
  } catch (error) {
    print(error.message);
  }
}

/**
 * Aborts the PaymentRequest initiated by testPaymentMethods().
 */
async function abort() {
  await request.abort();
  return await showPromise.catch((e) => {
    return e.name == 'AbortError';
  });
}

/**
 * Launches the PaymentRequest UI with Bob Pay and credit cards as payment
 * methods.
 */
function buy() {
  testPaymentMethods([
      {supportedMethods: 'https://bobpay.test'},
      {
        supportedMethods: 'basic-card',
        data: {supportedNetworks: ['visa', 'mastercard']},
      },
  ]);
}

/**
 * Launches the PaymentRequest UI with kylepay.test and basic-card as payment
 * methods. kylepay.test hosts an installable payment app.
 */
function testInstallableAppAndCard() {
  testPaymentMethods([
      {supportedMethods: 'https://kylepay.test/webpay'},
      {supportedMethods: 'basic-card'},
  ]);
}
