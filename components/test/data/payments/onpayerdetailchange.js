/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

let gPaymentResponse = null;
let gRetryPromise = null;

const bobPayMethod = Object.freeze({
  supportedMethods: 'https://bobpay.test',
});

/**
 * Launches the PaymentRequest UI
 */
function buy() {
  const options = {
    requestPayerEmail: true,
    requestPayerName: true,
    requestPayerPhone: true,
  };
  getPaymentResponseWithMethod(options, [bobPayMethod])
      .then(function(response) {
        gPaymentResponse = response;
        const eventPromise = new Promise(function(resolve) {
          gPaymentResponse.addEventListener('payerdetailchange', function(e) {
            e.updateWith({});
            resolve();
          });
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
function retry(validationErrors) {
  if (gPaymentResponse == null) {
    return;
  }

  gRetryPromise = gPaymentResponse.retry(validationErrors);
}
