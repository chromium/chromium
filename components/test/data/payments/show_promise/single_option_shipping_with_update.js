/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launch PaymentRequest with a show promise and a single pre-selected option
 * for shipping worldwide and a handler for shipping address change events that
 * does not change anything.
 * @param {string} supportedMethods - The payment method identifier.
 */
function buy(supportedMethods) {
  const finalizedDetails = {
    total: {label: 'Total', amount: {currency: 'USD', value: '1.00'}},
    shippingOptions: [{
      id: '1',
      label: 'Free shipping',
      amount: {currency: 'USD', value: '0.00'},
      selected: true,
    }],
  };

  try {
    const request = new PaymentRequest(
        [{supportedMethods}], {
          total: {
            label: 'PENDING TOTAL',
            amount: {currency: 'USD', value: '99.99'},
          },
          shippingOptions: [{
            id: '1',
            label: 'PENDING SHIPPING',
            amount: {currency: 'USD', value: '99.99'},
            selected: true,
          }],
        },
        {requestShipping: true});

    request.addEventListener('shippingaddresschange', function(evt) {
      evt.updateWith(finalizedDetails);
    });

    request
        .show(new Promise(function(resolve) {
          resolve(finalizedDetails);
        }))
        .then(function(result) {
          print(JSON.stringify(result.details));
          return result.complete('success');
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error.message);
  }
}
