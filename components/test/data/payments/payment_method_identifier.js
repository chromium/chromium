/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const basicCardMethod = { // eslint-disable-line no-unused-vars
  supportedMethods: 'basic-card',
};

const basicMastercardVisaMethod = { // eslint-disable-line no-unused-vars
  supportedMethods: 'basic-card',
  data: {
    supportedNetworks: ['mastercard', 'visa'],
  },
};

const basicVisaMethod = { // eslint-disable-line no-unused-vars
  supportedMethods: 'basic-card',
  data: {
    supportedNetworks: ['visa'],
  },
};

const basicMastercardMethod = { // eslint-disable-line no-unused-vars
  supportedMethods: 'basic-card',
  data: {
    supportedNetworks: ['mastercard'],
  },
};

const basicDebitMethod = { // eslint-disable-line no-unused-vars
  supportedMethods: 'basic-card',
  data: {
    supportedTypes: ['debit'],
  },
};

const alicePayMethod = { // eslint-disable-line no-unused-vars
  supportedMethods: 'https://alicepay.com/webpay',
};

const bobPayMethod = { // eslint-disable-line no-unused-vars
  supportedMethods: 'https://bobpay.com/webpay',
};

const defaultDetails = {
  total: {
    label: 'Total',
    amount: {
      currency: 'USD',
      value: '5.00',
    },
  },
};

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
    print(error);
  }
}

/**
 * Calls PaymentRequest.canMakePayment() and prints out the result.
 * @param {sequence<PaymentMethodData>} methodData The supported methods.
 * @private
 */
function checkCanMakePayment(methodData) { // eslint-disable-line no-unused-vars
  run(() => {
    const request = new PaymentRequest(methodData, defaultDetails);
    return request.canMakePayment();
  });
}

/**
 * Calls PaymentRequest.hasEnrolledInstrument() and prints out the result.
 * @param {sequence<PaymentMethodData>} methodData The supported methods.
 */
function checkHasEnrolledInstrument(methodData) { // eslint-disable-line no-unused-vars, max-len
  run(() => {
    const request = new PaymentRequest(methodData, defaultDetails);
    return request.hasEnrolledInstrument();
  });
}

/**
 * Calls PaymentRequest.show() and prints out the result.
 * @param {sequence<PaymentMethodData>} methodData The supported methods.
 * @private
 */
function buyHelper(methodData) {
  try {
    new PaymentRequest(methodData, defaultDetails)
        .show()
        .then(function(response) {
          response.complete('success')
              .then(function() {
                print(JSON.stringify(response, undefined, 2));
              })
              .catch(function(error) {
                print(error);
              });
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error);
  }
}

/**
 * Merchant requests payment via either "mastercard" or "basic-card" with "visa"
 * as the supported network.
 */
function buy() { // eslint-disable-line no-unused-vars
  buyHelper([basicMastercardVisaMethod]);
}
