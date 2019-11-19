/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */
/* global print:false */

/*
 * The Payment Request for this page.
 * @const
 */
var REQUEST = new PaymentRequest(
    [
      {supportedMethods: 'https://bobpay.com'},
      {supportedMethods: 'basic-card', data: {supportedMethods: ['visa']}},
    ],
    {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}});

/**
 * Show the Payment Request UI.
 */
function show() { // eslint-disable-line no-unused-vars
  try {
    REQUEST.show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
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

/**
 * Aborts the PaymentRequest UI.
 */
function abort() { // eslint-disable-line no-unused-vars
  try {
    REQUEST.abort()
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
