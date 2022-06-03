/*
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launch PaymentRequest with a show promise that resolves with an empty
 * dictionary.
 * @param {boolean} useUrlPaymentMethod - Whether URL payment method should be
 * used. Useful for payment handlers, which cannot use basic-card payment
 * method. By default, basic-card payment method is used.
 */
function buy(useUrlPaymentMethod) { // eslint-disable-line no-unused-vars
  try {
    let supportedMethods = 'basic-card';
    if (useUrlPaymentMethod) {
      supportedMethods = window.location.href;
    }
    var request = new PaymentRequest(
        [{supportedMethods}], {
          total: {
            label: 'Total',
            amount: {currency: 'USD', value: '3.00'},
          },
          displayItems: [{
            label: 'Display item',
            amount: {currency: 'USD', value: '1.00'},
          }],
          modifiers: [{
            supportedMethods,
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
