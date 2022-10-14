/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Creates a PaymentRequest for the specified card network.
 *
 * @param {string} network The supportedNetwork value to use.
 * @return {PaymentRequest} The new PaymentRequest created.
 */
function createPaymentRequest(network) {
  return new PaymentRequest(
      [{
        supportedMethods: 'basic-card',
        data: {
          supportedNetworks: [network],
        },
      }],
      {
        total: {
          label: 'Total',
          amount: {
            currency: 'USD',
            value: '5.00',
          },
        },
      });
}

/**
 * Runs |testFunction| and prints any result or error.
 *
 * @param {function} testFunction A function with no argument and returns a
 * Promise.
 */
function run(testFunction) {
  try {
    testFunction().then(print).catch(print);
  } catch (error) {
    print(error.message);
  }
}

/**
 * Checks for existence of a complete VISA credit card.
 */
function buy() {
  const request = createPaymentRequest('visa');
  run(() => {
    return request.canMakePayment();
  });
}

/**
 * Checks for existence of a complete MasterCard credit card.
 */
function otherBuy() {
  const request = createPaymentRequest('mastercard');
  run(() => {
    return request.canMakePayment();
  });
}

/**
 * Checks for existence of an enrolled instrument of the specified card network.
 *
 * @param {string} network The credit card network to check.
 */
function hasEnrolledInstrument(network) {
  const request = createPaymentRequest(network);
  run(() => {
    return request.hasEnrolledInstrument();
  });
}
