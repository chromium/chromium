/*
 * Copyright 2018 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

let gPaymentResponse = null;

/**
 * Launches the PaymentRequest UI
 */
function buy() {
  const options = {
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
function retry(validationErrors) {
  if (gPaymentResponse == null) {
    return;
  }

  gPaymentResponse.retry(validationErrors);
}
