/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */
/* global print:false */

// Global variable. Used by abort().
var request;

const bobPayMethod = Object.freeze({
  supportedMethods: 'https://bobpay.com',
});

const visaMethod = Object.freeze({
  supportedMethods: 'basic-card',
  data: {
    supportedNetworks: ['visa'],
  },
});

const defaultDetails = Object.freeze({
  total: {
    label: 'Total',
    amount: {
      currency: 'USD',
      value: '5.00',
    },
  },
});


/**
 * Do not query CanMakePayment before showing the Payment Request.
 */
function noQueryShow() { // eslint-disable-line no-unused-vars
  try {
    request = new PaymentRequest([bobPayMethod, visaMethod], defaultDetails);
    request.show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print(error);
              });
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Queries CanMakePayment and the shows the PaymentRequest after.
 */
async function queryShow() { // eslint-disable-line no-unused-vars
  try {
    request = new PaymentRequest([bobPayMethod, visaMethod], defaultDetails);
    print(await request.canMakePayment());
    print(await request.hasEnrolledInstrument());
    request.show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print(error);
              });
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Queries CanMakePayment but does not show the PaymentRequest after.
 */
async function queryNoShow() { // eslint-disable-line no-unused-vars
  try {
    request = new PaymentRequest([bobPayMethod, visaMethod], defaultDetails);
    print(await request.canMakePayment());
    print(await request.hasEnrolledInstrument());
  } catch (error) {
    print(error.message);
  }
}

/**
 * Aborts the PaymentRequest UI.
 */
function abort() { // eslint-disable-line no-unused-vars
  try {
    request.abort()
        .then(function() {
          print('Aborted');
        })
        .catch(function() {
          print('Cannot abort');
        });
  } catch (error) {
    print(error.message);
  }
}
