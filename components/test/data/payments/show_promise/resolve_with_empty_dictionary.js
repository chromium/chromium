/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launch PaymentRequest with a show promise that resolves with an empty
 * dictionary.
 */
function buy() { // eslint-disable-line no-unused-vars
  try {
    var request = new PaymentRequest(
        [{supportedMethods: 'basic-card'}], {
          total: {
            label: 'Total',
            amount: {currency: 'USD', value: '3.00'},
          },
          displayItems: [{
            label: 'Display item',
            amount: {currency: 'USD', value: '1.00'},
          }],
          modifiers: [{
            supportedMethods: 'basic-card',
            additionalDisplayItems: [{
              label: 'Modifier',
              pending: true,
              amount: {currency: 'USD', value: '1.00'},
            }],
          }],
          shippingOptions: [{
            label: 'Shipping option',
            id: 'shipping-option-identifier',
            selected: true,
            amount: {currency: 'USD', value: '1.00'},
          }],
        },
        {requestShipping: true});

    // Should NOT clear out any of the items.
    // Payment sheet should NOT display a message to "select an address",
    // because the shipping option in the constructor is selected and is not
    // cleared out.
    request.show(Promise.resolve({}))
        .then(function(result) {
          print(JSON.stringify({
            details: result.details,
            shippingOption: request.shippingOption,
          }));
          return result.complete('success');
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error.message);
  }
}
