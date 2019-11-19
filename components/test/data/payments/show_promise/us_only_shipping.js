/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launch PaymentRequest with a show promise and US-only shipping.
 */
function buy() { // eslint-disable-line no-unused-vars
  var detailsForUSAddress = {
    shippingOptions: [{
      id: '1',
      label: 'Free shipping',
      amount: {currency: 'USD', value: '0.00'},
      selected: true,
    }],
  };

  var detailsForNonUSAddress = {error: 'Cannot ship outside of US.'};

  try {
    var request = new PaymentRequest(
        [{supportedMethods: 'basic-card'}], {
          total: {
            label: 'PENDING TOTAL',
            amount: {currency: 'USD', value: '99.99'},
          },
        },
        {requestShipping: true});

    request.addEventListener('shippingaddresschange', function(evt) {
      if (request.shippingAddress.country === 'US') {
        evt.updateWith(detailsForUSAddress);
      } else {
        evt.updateWith(detailsForNonUSAddress);
      }
    });

    request
        .show(new Promise(function(resolve) {
          resolve({
            total: {label: 'Total', amount: {currency: 'USD', value: '1.00'}},
          });
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
