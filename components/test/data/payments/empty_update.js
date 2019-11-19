/*
 * Copyright 2017 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Invokes PaymentRequest with shipping and immediately rejects all shipping
 * addresses by calling updateWith({}), which is an "empty update."
 */
function buy() { // eslint-disable-line no-unused-vars
  try {
    var details = {
      total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
      displayItems: [
        {
          label: 'Pending shipping price',
          amount: {currency: 'USD', value: '0.00'},
          pending: true,
        },
        {label: 'Subtotal', amount: {currency: 'USD', value: '5.00'}},
      ],
    };

    var request = new PaymentRequest(
        [{supportedMethods: 'basic-card'}], details, {requestShipping: true});

    request.addEventListener('shippingaddresschange', function(evt) {
      evt.updateWith({});
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
