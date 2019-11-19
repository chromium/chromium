/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

var gPaymentResponse = null;
var gRetryPromise = null;

/**
 * Launches the PaymentRequest UI
 */
function buy() { // eslint-disable-line no-unused-vars
  var options = {
    requestPayerEmail: true,
    requestPayerName: true,
    requestPayerPhone: true,
  };
  getPaymentResponse(options)
      .then(function(response) {
        gPaymentResponse = response;
        var eventPromise = new Promise(function(resolve) {
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
function retry(validationErrors) { // eslint-disable-line no-unused-vars
  if (gPaymentResponse == null) {
    return;
  }

  gRetryPromise = gPaymentResponse.retry(validationErrors);
}
