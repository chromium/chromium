/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Creates two PaymentRequest objects for the given payment method and queries
 * their hasEnrolledInstrument() methods.
 * @param {string} supportedMethods - The payment method identifier to use.
 * @return {Promise<string>} - 'true' if both PaymentRequest objects return
 * 'true' in hasEnrolledInstrument() call. Otherwise 'false' or an error
 * message.
 */
async function hasEnrolledInstrumentInTwoPaymentRequestObjects(
    supportedMethods) {
  try {
    const methods = [{supportedMethods}];
    const details = {
      total: {label: 'Total', amount: {value: '0.01', currency: 'USD'}},
    };
    const request1 = new PaymentRequest(methods, details);
    const request2 = new PaymentRequest(methods, details);
    const promise1 = request1.hasEnrolledInstrument();
    const promise2 = request2.hasEnrolledInstrument();
    const results = await Promise.all([promise1, promise2]);
    return (
        results.every((result) => {
          return result === true;
        }) ?
            'true' :
            'false');
  } catch (e) {
    return e.message;
  }
}
