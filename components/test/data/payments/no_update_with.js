/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Builds a PaymentRequest that requests a shipping address.
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 * @return {PaymentRequest} - A new PaymentRequest object.
 */
function buildPaymentRequest(methodData) {
  try {
    return new PaymentRequest(
        methodData, {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          shippingOptions: [{
            selected: true,
            id: 'freeShipping',
            label: 'Free shipping',
            amount: {currency: 'USD', value: '0.00'},
          }],
        },
        {requestShipping: true});
  } catch (error) {
    print(error.message);
  }
}

/**
 * Shows the PaymentRequest.
 * @param {PaymentRequest} pr - The PaymentRequest object to show.
 */
function showPaymentRequest(pr) {
  pr.show()
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
}

/**
 * Show a PaymentRequest using methodData that requests a shipping address, but
 * has no listeners.
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
 function buyWithoutListenersWithMethods(methodData) {
  showPaymentRequest(buildPaymentRequest(methodData));
}

/**
 * Show a PaymentRequest using methodData that requests a shipping address, but
 * listeners don't call updateWith().
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
 function buyWithoutCallingUpdateWithWithMethods(methodData) {
  const pr = buildPaymentRequest(methodData);
  pr.addEventListener('shippingaddresschange', function(evt) {
    print('shippingaddresschange');
  });
  pr.addEventListener('shippingoptionchange', function(evt) {
    print('shippingoptionchange');
  });
  showPaymentRequest(pr);
}

/**
 * Show a PaymentRequest using methodData that requests a shipping address, but
 * listeners don't use promises to update the UI.
 * @param {sequence<PaymentMethodData>} methodData An array of payment method
 *        objects.
 */
 function buyWithoutPromisesWithMethods(methodData) {
  const pr = buildPaymentRequest(methodData);
  const updatedDetails = {
    total: {label: 'Updated total', amount: {currency: 'USD', value: '10.00'}},
    shippingOptions: [{
      selected: true,
      id: 'updatedShipping',
      label: 'Updated shipping',
      amount: {currency: 'USD', value: '5.00'},
    }],
  };
  pr.addEventListener('shippingaddresschange', function(evt) {
    evt.updateWith(updatedDetails);
  });
  pr.addEventListener('shippingoptionchange', function(evt) {
    evt.updateWith(updatedDetails);
  });
  showPaymentRequest(pr);
}
