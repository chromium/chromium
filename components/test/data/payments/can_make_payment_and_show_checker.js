/*
 * Copyright 2025 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const kDetails = {
  total: {label: 'TEST', amount: {currency: 'USD', value: '0.01'}},
};

let paymentRequest;

/**
 * Initializes the global `paymentRequest` variable and checks whether the given
 * payment method can make payments.
 * @param {string} method - The payment method identifier to check.
 * @return {string} - 'true', 'false', or an error message on failure.
 */
async function initAndCheckCanMakePayment(method) {
  try {
    paymentRequest = new PaymentRequest([{supportedMethods: method}], kDetails);
    const result = await paymentRequest.canMakePayment();
    return result ? 'true' : 'false';
  } catch (e) {
    return e.message;
  }
}

/**
 * Checks the result of calling PaymentRequest.hasEnrolledInstrument() on the
 * global `paymentRequest` variable, which must be initialized for this method
 * to work.
 * @return {string} - 'true', 'false', or an error message on failure.
 */
async function checkHasEnrolledInstrument(method) {
  try {
    const result = await paymentRequest.hasEnrolledInstrument();
    return result ? 'true' : 'false';
  } catch (e) {
    return e.message;
  }
}

/**
 * Checks the result of calling PaymentRequest.show() on the global
 * `paymentRequest` variable, which must be initialized for this method to work.
 * @return {string} - The string 'success' or an error name on failure.
 */
async function checkShowResult() {
  try {
    const response = await paymentRequest.show();
    await response.complete();
    return 'success';
  } catch (e) {
    return e.name;
  }
}
