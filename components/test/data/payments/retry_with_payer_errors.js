/*
 * Copyright 2018 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

var gPaymentResponse = null;

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
        print(JSON.stringify(gPaymentResponse, undefined, 2));
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

  gPaymentResponse.addEventListener('payerdetailchange', function(e) {
    print(JSON.stringify(gPaymentResponse, undefined, 2));
  });

  gPaymentResponse.retry(validationErrors);
}
