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
    requestShipping: true,
  };
  getPaymentResponse(options)
      .then(function(response) {
        gPaymentResponse = response;
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

  gPaymentResponse.retry(validationErrors);
}
