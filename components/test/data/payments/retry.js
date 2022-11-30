/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

var gPaymentResponse = null;
var gRetryPromise = null;

/**
 * Launches the PaymentRequest UI
 */
function buy() { // eslint-disable-line no-unused-vars
  buyWithMethod([{supportedMethods: 'basic-card'}]);
}

/**
 * Launches the PaymentRequest UI
 */
function buyWithUrlMethod() { // eslint-disable-line no-unused-vars
  buyWithMethod([
    {supportedMethods: 'https://bobpay.com'},
    {supportedMethods: 'https://kylepay.com/webpay'},
  ]);
}

/**
 * Launches the PaymentRequest UI
 * @param {string} method The payment method to request
 */
function buyWithMethod(method) { // eslint-disable-line no-unused-vars
  var options = {
    requestPayerEmail: true,
    requestPayerName: true,
    requestPayerPhone: true,
    requestShipping: true,
  };
  getPaymentResponseWithMethod(options, method)
      .then(function(response) {
        gPaymentResponse = response;
        var eventPromise = new Promise(function(resolve) {
          gPaymentResponse.addEventListener('payerdetailchange', resolve);
        });
        eventPromise.then(function() {
          gRetryPromise.then(function() {
            print(JSON.stringify(gPaymentResponse, undefined, 2));
            gPaymentResponse.complete('success');
          });
        });
      });
}

/**
 * Retry PaymentRequest UI with indicating validation error messages.
 *
 * @param {PaymentValidationErrors} validationErrors Represent validation errors
 */
function retry(validationErrors) { // eslint-disable-line no-unused-vars
  if (gPaymentResponse == null) {
    return;
  }

  gRetryPromise = gPaymentResponse.retry(validationErrors);
}
