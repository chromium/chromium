/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launch PaymentRequest by resolving the promised passed into the shoe() method
 * with empty lists of display items, modifiers, and shipping options.
 */
function buy() { // eslint-disable-line no-unused-vars
  try {
    var request = new PaymentRequest(
        [{supportedMethods: 'basic-card'}], {
          total: {label: 'Total', amount: {currency: 'USD', value: '1.00'}},
          displayItems: [{
            label: 'PENDING DISPLAY ITEM',
            pending: true,
            amount: {currency: 'USD', value: '99.99'},
          }],
          modifiers: [{
            supportedMethods: 'basic-card',
            additionalDisplayItems: [{
              label: 'PENDING ADDITIONAL DISPLAY ITEM',
              pending: true,
              amount: {currency: 'USD', value: '88.88'},
            }],
          }],
          shippingOptions: [{
            label: 'PENDING SHIPPING OPTION',
            id: 'shipping-option-identifier',
            selected: true,
            amount: {currency: 'USD', value: '77.77'},
          }],
        },
        {requestShipping: true});

    // Should clear out everything except the total.
    // Payment sheet should display a message to "select an address", because
    // the shipping option from the constructor is cleared out.
    request.show({displayItems: [], modifiers: [], shippingOptions: []})
        .then(function(result) {
          print(JSON.stringify(result.details));
          return result.complete('success');
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error);
  }
}
