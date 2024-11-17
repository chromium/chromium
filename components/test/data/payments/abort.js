/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

let request;

/**
 * Launches the PaymentRequest UI with basic-card request.
 */
function buy() {
  buyWithMethods([{
      supportedMethods: 'basic-card',
      data: {supportedNetworks: ['visa']}}]);
}

/**
 * Launches the PaymentRequest UI.
 * @param {Array<Object>} methodData An array of payment method objects.
 */
function buyWithMethods(methodData) {
  try {
    request = new PaymentRequest(
        methodData,
        {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}});
    request.show().catch(function(error) {
      print(error);
    });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Aborts the PaymentRequest UI.
 */
function abort() {
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
