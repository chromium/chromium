/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */

/**
 * Launches the PaymentRequest UI that requests contact details.
 */
function buy() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [
          {supportedMethods: 'https://bobpay.com'},
          {
            supportedMethods: 'basic-card',
            data: {supportedNetworks: ['amex', 'visa']},
          },
        ],
        {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}}, {
          requestPayerName: true,
          requestPayerEmail: true,
          requestPayerPhone: true,
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(
                    resp.payerName + '<br>' + resp.payerEmail + '<br>' +
                    resp.payerPhone + '<br>' + resp.methodName + '<br>' +
                    JSON.stringify(resp.details, undefined, 2));
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
