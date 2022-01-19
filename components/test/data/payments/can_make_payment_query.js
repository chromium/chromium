/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */
/* global print:false */

const bobPayMethod = {supportedMethods: 'https://bobpay.com'};

const visaMethod = {
  supportedMethods: 'basic-card',
  data: {
    supportedNetworks: ['visa'],
  },
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
function buy() { // eslint-disable-line no-unused-vars
  buyWithMethods([bobPayMethod, visaMethod]);
}

/**
 * Checks for existence of the given methods.
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
function buyWithMethods(methodData) {
  var request = new PaymentRequest(methodData, defaultDetails);
  run(() => {
    return request.canMakePayment();
  });
}

/**
 * Show payment UI for Bob Pay or a complete credit card.
 */
function show() { // eslint-disable-line no-unused-vars
  showWithMethods([bobPayMethod, visaMethod]);
}

/**
 * Show payment UI for the given methods.
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
function showWithMethods(methodData) {
  var request = new PaymentRequest(methodData, defaultDetails);
  run(() => {
    return request.show();
  });
}

/**
 * Checks for enrolled instrument of Bob Pay or a complete credit card.
 */
function hasEnrolledInstrument() { // eslint-disable-line no-unused-vars
  hasEnrolledInstrumentWithMethods([bobPayMethod, visaMethod]);
}

/**
 * Checks for enrolled instrument of the given methods.
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
 function hasEnrolledInstrumentWithMethods(methodData) {
  var request = new PaymentRequest(methodData, defaultDetails);
  run(() => {
    return request.hasEnrolledInstrument();
  });
}
