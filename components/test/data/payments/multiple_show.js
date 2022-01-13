/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */

var request;
var request2;

/**
 * Show a Payment Request.
 */
function buy() { // eslint-disable-line no-unused-vars
  buyWithMethods([
    {supportedMethods: 'https://bobpay.com'},
    {
      supportedMethods: 'basic-card',
      data: {supportedNetworks: ['visa']},
    },
  ]);
}

/**
 * Show a Payment Request with given methods.
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
function buyWithMethods(methodData) {
  try {
    request = new PaymentRequest(
        methodData,
        {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}});
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
 * Try to re-show an existing Payment Request.
 */
function showAgain() { // eslint-disable-line no-unused-vars
  try {
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
 * Show a second Payment Request.
 */
function showSecondRequest() { // eslint-disable-line no-unused-vars
  showSecondRequestWithMethods([
    {supportedMethods: 'https://bobpay.com'},
    {
      supportedMethods: 'basic-card',
      data: {supportedNetworks: ['visa']},
    },
  ]);
}

/**
 * Show a second Payment Request with given methods.
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
function showSecondRequestWithMethods(methodData) {
  try {
    request2 = new PaymentRequest(
        methodData,
        {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}});
    request2.show()
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
