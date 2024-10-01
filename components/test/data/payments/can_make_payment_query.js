/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const bobPayMethod = {supportedMethods: 'https://bobpay.test'};

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
 * Checks for existence of Bob Pay or a complete credit card.
 */
function buy() {
  buyWithMethods([bobPayMethod]);
}

/**
 * Checks for availability of the given method.
 * @param {string} methodName - The payment method name to check.
 * @return {Promise<bool|string>} Either the boolean indicating whether the
 * given payment method is available, or an error message string.
 */
async function checkCanMakePayment(methodName) {
  try {
    return new PaymentRequest([{supportedMethods: methodName}], defaultDetails)
        .canMakePayment();
  } catch (e) {
    return e.toString();
  }
}

/**
 * Checks for enrolled instrument presence of the given method.
 * @param {string} methodName - The payment method name to check.
 * @return {Promise<bool|string>} Either the boolean indicating whether the
 * given payment method has an enrolled instrument, or an error message string.
 */
async function checkHasEnrolledInstrument(methodName) {
  try {
    return new PaymentRequest([{supportedMethods: methodName}], defaultDetails)
        .hasEnrolledInstrument();
  } catch (e) {
    return e.toString();
  }
}

/**
 * Returns the payment response from the payment handler.
 * @param {string} methodName - The payment method name to invoke.
 * @return {Promise<string>} The payment response from the payment handler, or
 * an error message.
 */
async function getShowResponse(methodName) {
  try {
    const request =
        new PaymentRequest([{supportedMethods: methodName}], defaultDetails);
    const response = await request.show();
    await response.complete('success');
    return JSON.stringify(response);
  } catch (e) {
    return e.toString();
  }
}

/**
 * Checks for existence of the given methods.
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
function buyWithMethods(methodData) {
  const request = new PaymentRequest(methodData, defaultDetails);
  run(() => {
    return request.canMakePayment();
  });
}

/**
 * Show payment UI for Bob Pay or a complete credit card.
 */
function show() {
  showWithMethods([bobPayMethod]);
}

/**
 * Show payment UI for the given methods.
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
function showWithMethods(methodData) {
  const request = new PaymentRequest(methodData, defaultDetails);
  run(() => {
    return request.show();
  });
}

/**
 * Checks for enrolled instrument of Bob Pay or a complete credit card.
 */
function hasEnrolledInstrument() {
  hasEnrolledInstrumentWithMethods([bobPayMethod]);
}

/**
 * Checks for enrolled instrument of the given methods.
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
 function hasEnrolledInstrumentWithMethods(methodData) {
   const request = new PaymentRequest(methodData, defaultDetails);
   run(() => {
     return request.hasEnrolledInstrument();
   });
}
