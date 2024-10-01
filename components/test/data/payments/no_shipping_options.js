/*
 * Copyright 2020 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI that is invalid because requestShipping is
 * true but no shipping options are provided.
 */
function buy() {  // eslint-disable-line
  try {
    const details = {
      total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
    };
    const request = new PaymentRequest(
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
