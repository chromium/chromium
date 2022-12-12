/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const basicCardMethod = {
  supportedMethods: 'basic-card',
};

const basicMastercardVisaMethod = {
  supportedMethods: 'basic-card',
  data: {
    supportedNetworks: ['mastercard', 'visa'],
  },
};

const basicVisaMethod = {
  supportedMethods: 'basic-card',
  data: {
    supportedNetworks: ['visa'],
  },
};

const basicMastercardMethod = {
  supportedMethods: 'basic-card',
  data: {
    supportedNetworks: ['mastercard'],
  },
};

const alicePayMethod = {
  supportedMethods: 'https://alicepay.test/webpay',
};

const bobPayMethod = {
  supportedMethods: 'https://bobpay.test/webpay',
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
function checkCanMakePayment(methodData) {
  run(() => {
    const request = new PaymentRequest(methodData, defaultDetails);
    return request.canMakePayment();
  });
}

/**
 * Calls PaymentRequest.hasEnrolledInstrument() and prints out the result.
 * @param {sequence<PaymentMethodData>} methodData The supported methods.
 */
function checkHasEnrolledInstrument(methodData) {
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
function buy() {
  buyHelper([basicMastercardVisaMethod]);
}
