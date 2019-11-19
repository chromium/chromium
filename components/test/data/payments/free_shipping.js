/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */
/* global print:false */

/**
 * Launches the PaymentRequest UI that offers free shipping worldwide.
 */
function buy() { // eslint-disable-line no-unused-vars
  try {
    var details = {
      total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
      shippingOptions: [{
        id: 'freeShippingOption',
        label: 'Free global shipping',
        amount: {currency: 'USD', value: '0'},
        selected: true,
      }],
    };
    var request = new PaymentRequest(
        [{
          supportedMethods: 'basic-card',
          data: {
            supportedNetworks: [
              'visa',
              'unionpay',
              'mir',
              'mastercard',
              'jcb',
              'discover',
              'diners',
              'amex',
            ],
          },
        }],
        details, {requestShipping: true});
    request.addEventListener('shippingaddresschange', function(e) {
      e.updateWith(new Promise(function(resolve) {
        // No changes in price based on shipping address change.
        resolve(details);
      }));
    });
    request.show()
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
