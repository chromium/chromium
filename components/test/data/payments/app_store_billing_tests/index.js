/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const methodName = window.location.origin + '/method_manifest.json';
let request;
let supportedInstruments = [];

/**
 * Install a payment app.
 * @return {string} - a message indicating whether the installation is
 *  successful.
 */
async function install() {
  info('installing');

  await navigator.serviceWorker.register('empty_app.js');
  const registration = await navigator.serviceWorker.ready;
  if (!registration.paymentManager) {
    return 'No payment handler capability in this browser. Is' +
        'chrome://flags/#service-worker-payment-apps enabled?';
  }

  if (!registration.paymentManager.instruments) {
    return 'Payment handler is not fully implemented. ' +
        'Cannot set the instruments.';
  }
  await registration.paymentManager.instruments.set('instrument-key', {
    // Chrome uses name and icon from the web app manifest
    name: 'MaxPay',
    method: methodName,
  });
  return 'success';
}

/**
 * Add a payment method to the payment request.
 * @param {string} method - the payment method.
 * @return {string} - a message indicating whether the operation is successful.
 */
function addSupportedMethod(method) {
  info('addSupportedMethod: ' + method);
  supportedInstruments.push({
    supportedMethods: [
      method,
    ],
  });
  return 'success';
}

/**
 * Create a PaymentRequest.
 * @return {string} - a message indicating whether the operation is successful.
 */
function createPaymentRequest() {
  info('createPaymentRequest: ' + JSON.stringify(supportedInstruments));
  const details = {
    total: {
      label: 'Donation',
      amount: {
        currency: 'USD',
        value: '55.00',
      },
    },
  };
  request = new PaymentRequest(supportedInstruments, details);
  return 'success';
}

/**
 * Check whether payments can be made.
 * @return {string} - "true", "false", or an error message.
 */
async function canMakePayment() {
  info('canMakePayment');
  try {
    const result = await request.canMakePayment();
    return (result ? 'true' : 'false');
  } catch (e) {
    info('canMakePayment error: ' + e.toString());
    return e.toString();
  }
}

/**
 * Show the payment sheet.
 * @return {string} - a message indicating whether the operation is successful.
 */
async function show() {
  info('show');
  try {
    return await request.show();
  } catch (e) {
    info('show error: ' + e.toString());
    return e.toString();
  }
}
