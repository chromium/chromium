/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Checks whether the given payment method has an enrolled instrument.
 * @param {string} method - The payment method identifier to check.
 * @return {string} - 'true', 'false', or error message on failure.
 */
async function hasEnrolledInstrument(method) {
  return hasEnrolledInstrumentForMethodData([{supportedMethods: method}]);
}

/**
 * Creates a PaymentRequest with `methodData` and checks hasEnrolledInstrument.
 * @param {object} methodData - The payment method data to build the request.
 * @return {string} - 'true', 'false', or error message on failure.
 */
async function hasEnrolledInstrumentForMethodData(methodData) {
  try {
    const request = new PaymentRequest(
        methodData,
        {total: {label: 'TEST', amount: {currency: 'USD', value: '0.01'}}});
    const result = await request.hasEnrolledInstrument();
    return result ? 'true' : 'false';
  } catch (e) {
    return e.message;
  }
}
