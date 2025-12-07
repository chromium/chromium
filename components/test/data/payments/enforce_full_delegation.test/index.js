/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const CURRENT_URL = window.location.origin + window.location.pathname;
const METHOD_NAME = CURRENT_URL.substring(0, CURRENT_URL.lastIndexOf('/')) +
    '/method_manifest.json';
const SW_SRC_URL = 'app.js';
let request;
const supportedInstruments = [];

/**
 * Delegates handling of the provided options to the payment handler.
 * @param {Array<string>} delegations The list of payment options to delegate.
 * @return {string} The 'success' or error message.
 */
async function enableDelegations(delegations) {
  info('enableDelegations: ' + JSON.stringify(delegations));
  try {
    await navigator.serviceWorker.ready;
    const registration =
        await navigator.serviceWorker.getRegistration(SW_SRC_URL);
    if (!registration) {
      return 'The payment handler is not installed.';
    }
    if (!registration.paymentManager) {
      return 'PaymentManager API not found.';
    }
    if (!registration.paymentManager.enableDelegations) {
      return 'PaymentManager does not support enableDelegations method';
    }

    await registration.paymentManager.enableDelegations(delegations);
    return 'success';
  } catch (e) {
    return e.toString();
  }
}

/**
 * Add payment methods to the payment request.
 * @param {string[]} methods - the payment methods.
 * @return {string} - a message indicating whether the operation is successful.
 */
function addSupportedMethods(methods) {
  info('addSupportedMethods: ' + JSON.stringify(methods));
  methods.forEach((method)=>{
    supportedInstruments.push({
      supportedMethods: [
        method,
      ],
    });
  });
  return 'success';
}

/**
 * Add the payment method of this test to the payment request.
 * @return {string} - a message indicating whether the operation is successful.
 */
function addDefaultSupportedMethod() {
  return addSupportedMethods([METHOD_NAME]);
}

/**
 * Create a PaymentRequest.
 * @param {PaymentOptions} options - the payment options.
 * @return {string} - a message indicating whether the operation is successful.
 */
function createPaymentRequestWithOptions(options) {
  info('createPaymentRequestWithOptions: ' +
      JSON.stringify(supportedInstruments) + ', ' + JSON.stringify(options));
  const details = {
    total: {
      label: 'Donation',
      amount: {
        currency: 'USD',
        value: '55.00',
      },
    },
  };
  request = new PaymentRequest(supportedInstruments, details, options);
  return 'success';
}

/**
 * Show the payment sheet. This method is not blocking.
 * @return {string} - a message indicating whether the operation is successful.
 */
function show() {
  info('show');
  request.show().then((response) => {
    info('complete: status=' + response.details.status + ', payerName='
        + response.payerName);
    response.complete(response.details.status).then(() => {
      info('complete success');
    }).catch((e) => {
      info('complete error: ' + e);
    }).finally(() => {
      info('show finished');
    });
  }).catch((e) => {
    info('show error: ' + e);
  });
  info('show on going');
  return 'success';
}
