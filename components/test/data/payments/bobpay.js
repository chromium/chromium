/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */

/**
 * Builds a payment request with URL based payment methods.
 * @return {!PaymentRequest} A payment request with URL based payment methods.
 * @private
 */
function buildPaymentRequest() {
  return new PaymentRequest(
      [
        {supportedMethods: 'https://bobpay.com'},
        {supportedMethods: 'https://alicepay.com'},
      ],
      {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}});
}

/**
 * Launches the PaymentRequest UI with Bob Pay as one of multiple payment
 * methods.
 */
function buy() { // eslint-disable-line no-unused-vars
  try {
    buildPaymentRequest()
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(
                    resp.methodName + '<br>' +
                    JSON.stringify(resp.details, undefined, 2));
              })
              .catch(function(error) {
                print('complete() rejected<br>' + error);
              });
        })
        .catch(function(error) {
          print('show() rejected<br>' + error);
        });
  } catch (error) {
    print('exception thrown<br>' + error);
  }
}

/**
 * Queries CanMakePayment but does not show the PaymentRequest after.
 */
function canMakePayment() { // eslint-disable-line no-unused-vars
  try {
    buildPaymentRequest()
        .canMakePayment()
        .then(function(result) {
          print(result);
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print('exception thrown<br>' + error);
  }
}
