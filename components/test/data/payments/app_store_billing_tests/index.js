/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const methodName = window.location.origin + '/method_manifest.json';
let request;
const supportedInstruments = [];

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
