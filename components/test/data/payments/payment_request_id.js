/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI including a details.id and prints the
 * resulting requestId.
 */
function buy() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [{supportedMethods: 'basic-card', data: {supportedNetworks: ['visa']}}],
        {
          id: 'my_payment_id',
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(resp.requestId);
              })
              .catch(function(error) {
                print(error.message);
              });
        })
        .catch(function(error) {
          print(error.message);
        });
  } catch (error) {
    print(error.message);
  }
}
