/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Builds a PaymentRequest for 'basic-card' with the given options.
 * @param {PaymentOptions} options - The payment options to use.
 * @return {PaymentRequest} The new PaymentRequest object.
 */
function buildPaymentRequest(options) {
  return new PaymentRequest(
      [{supportedMethods: 'basic-card'}],
      {total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}}},
      options);
}

/**
 * Checks the hasEnrolledInstrument() value for 'basic-card' with the given
 * options.
 * @param {PaymentOptions} options - The payment options to use.
 * @return {Promise<boolean|string>} The boolean value of
 * hasEnrolledInstrument() or the error message string.
 */
async function hasEnrolledInstrument(options) { // eslint-disable-line no-unused-vars,max-len
  try {
    return await buildPaymentRequest(options).hasEnrolledInstrument();
  } catch (e) {
    return e.toString();
  }
}

/**
 * Runs the show() method for 'basic-card' with the given options.
 * @param {PaymentOptions} options - The payment options to use.
 * @return {Promise<string>} The error message string, if any.
 */
async function show(options) { // eslint-disable-line no-unused-vars
  try {
    await buildPaymentRequest(options).show();
    return '';
  } catch (e) {
    return e.toString();
  }
}
