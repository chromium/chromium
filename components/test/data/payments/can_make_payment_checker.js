/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const kDetails = {
  total: {label: 'TEST', amount: {currency: 'USD', value: '0.01'}},
};

/**
 * Checks whether the given payment method can make payments.
 * @param {string} method - The payment method identifier to check.
 * @return {string} - 'true', 'false', or error message on failure.
 */
async function canMakePayment(method) {
  try {
    const request = new PaymentRequest([{supportedMethods: method}], kDetails);
    const result = await request.canMakePayment();
    return result ? 'true' : 'false';
  } catch (e) {
    return e.message;
  }
}

/**
 * Creates a PaymentRequest with |methodData| and checks canMakePayment.
 * @param {object} methodData - The payment method data to build the request.
 * @return {string} - 'true', 'false', or error message on failure.
 */
async function canMakePaymentForMethodData(methodData) {
  try {
    const request = new PaymentRequest(methodData, kDetails);
    const result = await request.canMakePayment();
    return result ? 'true' : 'false';
  } catch (e) {
    return e.message;
  }
}

/**
 * Creates a PaymentRequest with |methodData|, checks canMakePayment twice, and
 * returns the second value.
 * @param {object} methodData - The payment method data to build the request.
 * @return {string} - 'true', 'false', or error message on failure.
 */
async function canMakePaymentForMethodDataTwice(methodData) {
  try {
    const request = new PaymentRequest(methodData, kDetails);
    await request.canMakePayment(); // Discard first result.
    const result = await request.canMakePayment();
    return result ? 'true' : 'false';
  } catch (e) {
    return e.message;
  }
}
