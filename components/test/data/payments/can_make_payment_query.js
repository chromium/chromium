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
  var request = new PaymentRequest([bobPayMethod, visaMethod], defaultDetails);
  run(() => {
    return request.canMakePayment();
  });
}

/**
 * Checks for enrolled instrument of Bob Pay or a complete credit card.
 */
function hasEnrolledInstrument() { // eslint-disable-line no-unused-vars
  var request = new PaymentRequest([bobPayMethod, visaMethod], defaultDetails);
  run(() => {
    return request.hasEnrolledInstrument();
  });
}
