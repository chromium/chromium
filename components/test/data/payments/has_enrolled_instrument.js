/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Builds a PaymentRequest for the given method with the given options.
 * @param {PaymentOptions} options - The payment options to use.
 * @param {DOMString} method - The payment method to use. Optional. If not
 * specified, then 'basic-card' is used.
 * @return {PaymentRequest} The new PaymentRequest object.
 */
function buildPaymentRequest(options, method) {
  if (!method) {
    method = 'basic-card';
  }
  return new PaymentRequest(
      [{supportedMethods: method}],
      {total: {label: 'Total', amount: {currency: 'USD', value: '0.01'}}},
      options);
}

/**
 * Checks the hasEnrolledInstrument() value for the given payment method with
 * the given options.
 * @param {PaymentOptions} options - The payment options to use.
 * @param {DOMString} method - The payment method to use. Optional. If not
 * specified, then 'basic-card' is used.
 * @return {Promise<boolean|string>} The boolean value of
 * hasEnrolledInstrument() or the error message string.
 */
async function hasEnrolledInstrument(options, method) {
  try {
    return await buildPaymentRequest(options, method).hasEnrolledInstrument();
  } catch (e) {
    return e.toString();
  }
}

/**
 * Runs the show() method for 'basic-card' with the given options.
 * @param {PaymentOptions} options - The payment options to use.
 * @return {Promise<string>} The error message string, if any.
 */
async function show(options) {
  try {
    await buildPaymentRequest(options).show();
    return '';
  } catch (e) {
    return e.toString();
  }
}

/**
 * A version of show(options) that inserts a delay between creating the request
 * and calling request.show().
 * This is a regression test for https://crbug.com/1028114.
 * @param {PaymentOptions} options - The payment options to use.
 * @return {Promise<string>} The error message string, if any.
 */
async function delayedShow(options) {
  const request = buildPaymentRequest(options);

  try {
    // Block on hasEnrolledInstrument() to make sure when show() is called,
    // all instruments are available.
    await request.hasEnrolledInstrument();
    await request.show();
    return '';
  } catch (e) {
    return e.toString();
  }
}
