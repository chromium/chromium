/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const defaultDetails = {
  total: {
    label: 'Total',
    amount: {
      currency: 'USD',
      value: '5.00',
    },
  },
};

const bobPayMethod = {
  supportedMethods: 'https://bobpay.test',
  data: {
    'bobPayParameter': '1',
  },
};

const alicePayMethod = {
  supportedMethods: 'https://alicepay.test',
  data: {
    'alicePayParameter': '2',
  },
};

let first;
let second;

/**
 * Sets the |first| variable and prints both |first| and |second| only if both
 * were set.
 * @param {object} result The object to print.
 */
function printFirst(result) {
  first = result.toString();
  if (first && second) {
    print(first + ', ' + second);
  }
}

/**
 * Sets the |second| variable and prints both |first| and |second| only if both
 * were set.
 * @param {object} result The object to print.
 */
function printSecond(result) {
  second = result.toString();
  if (first && second) {
    print(first + ', ' + second);
  }
}

/**
 * Runs |testFunction| and logs any result or error using |logger|.
 * @param {function} testFunction A function with no argument and returns a
 * Promise.
 * @param {function} logger A function that takes one argument.
 */
function run(testFunction, logger) {
  try {
    testFunction().then(logger).catch(logger);
  } catch (error) {
    logger(error);
  }
}

/**
 * Checks for existence of Bob Pay twice, with the same payment method specific
 * parameters.
 */
function buy() {
  first = null;
  second = null;

  const request1 = new PaymentRequest([bobPayMethod], defaultDetails);
  run(() => {
    return request1.canMakePayment();
  }, printFirst);

  const request2 = new PaymentRequest([bobPayMethod], defaultDetails);
  run(() => {
    return request2.canMakePayment();
  }, printSecond);
}

/**
 * Checks for existence of Bob Pay and AlicePay.
 */
function otherBuy() {
  first = null;
  second = null;

  const request1 = new PaymentRequest([bobPayMethod], defaultDetails);
  run(() => {
    return request1.canMakePayment();
  }, printFirst);

  const request2 = new PaymentRequest([alicePayMethod], defaultDetails);
  run(() => {
    return request2.canMakePayment();
  }, printSecond);
}

/**
 * Checks for existence of an enrolled instrument for BobPay and AlicePay.
 */
function hasEnrolledInstrument() {
  first = null;
  second = null;

  const request1 = new PaymentRequest([bobPayMethod], defaultDetails);
  run(() => {
    return request1.hasEnrolledInstrument();
  }, printFirst);

  const request2 = new PaymentRequest([alicePayMethod], defaultDetails);
  run(() => {
    return request2.hasEnrolledInstrument();
  }, printSecond);
}
