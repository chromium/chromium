/*
 * Copyright 2018 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

var gPaymentResponse = null;
var gValidationErrors = null;

/**
 * Launches the PaymentRequest UI
 */
function buy() { // eslint-disable-line no-unused-vars
  buyWithMethods([{supportedMethods: 'basic-card'}]);
}

/**
 * Launches the PaymentRequest UI
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
function buyWithMethods(methodData) {
  var options = {};
  getPaymentResponseWithMethod(options, methodData).then(function(response) {
    if (gValidationErrors != null) {
      // retry() has been called before PaymentResponse promise resolved.
      response.retry(gValidationErrors);
    }
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
    // retry() has been called before PaymentResponse promise resolved.
    gValidationErrors = validationErrors;
    return;
  }

  gPaymentResponse.retry(validationErrors);
}
