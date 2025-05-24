/*
 * Copyright 2019 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Builds a PaymentRequest that requests a shipping address.
 * @param {String} paymentMethod - the payment method to be used.
 * @return {PaymentRequest} - A new PaymentRequest object.
 */
function buildPaymentRequest(paymentMethod) {
  try {
    return new PaymentRequest(
      [{supportedMethods: paymentMethod}], {
      total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
      displayItems: [
        {label: 'Item1', amount: {currency: 'USD', value: '2.00'}},
        {label: 'Item2', amount: {currency: 'USD', value: '3.00'}},
          ],
          shippingOptions: [{
            selected: true,
            id: 'freeShipping',
            label: 'Free shipping',
            amount: {currency: 'USD', value: '0.00'},
          }],
          modifiers: [{
            supportedMethods: paymentMethod,
            additionalDisplayItems: [{
              label: 'Discount',
              amount: {currency: 'USD', value: '0.00'},
        }],
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
 * Calls updateWith() with {}
 * @param {String} paymentMethod - the payment method to be used.
 */
function updateWithEmpty(paymentMethod) {
  const pr = buildPaymentRequest(paymentMethod);
  const updatedDetails = {};
  pr.addEventListener('shippingaddresschange', function(e) {
    e.updateWith(updatedDetails);
  });
  pr.addEventListener('shippingoptionchange', function(e) {
    e.updateWith(updatedDetails);
  });
  showPaymentRequest(pr);
}

/**
 * Calls updateWith() with total
 * @param {String} paymentMethod - the payment method to be used.
 */
function updateWithTotal(paymentMethod) {
  const pr = buildPaymentRequest(paymentMethod);
  const updatedDetails = {
    total: {label: 'Updated total', amount: {currency: 'USD', value: '10.00'}},
  };
  pr.addEventListener('shippingaddresschange', function(e) {
    e.updateWith(updatedDetails);
  });
  pr.addEventListener('shippingoptionchange', function(e) {
    e.updateWith(updatedDetails);
  });
  showPaymentRequest(pr);
}

/**
 * Calls updateWith() with displayItems
 * @param {String} paymentMethod - the payment method to be used.
 */
function updateWithDisplayItems(paymentMethod) {
  const pr = buildPaymentRequest(paymentMethod);
  const updatedDetails = {
    displayItems: [
      {label: 'Item1', amount: {currency: 'USD', value: '3.00'}},
      {label: 'Item2', amount: {currency: 'USD', value: '2.00'}},
    ],
  };
  pr.addEventListener('shippingaddresschange', function(e) {
    e.updateWith(updatedDetails);
  });
  pr.addEventListener('shippingoptionchange', function(e) {
    e.updateWith(updatedDetails);
  });
  showPaymentRequest(pr);
}

/**
 * Calls updateWith() with shipping options
 * @param {String} paymentMethod - the payment method to be used.
 */
function updateWithShippingOptions(paymentMethod) {
  const pr = buildPaymentRequest(paymentMethod);
  const updatedDetails = {
    shippingOptions: [{
      selected: true,
      id: 'updatedShipping',
      label: 'Updated shipping',
      amount: {currency: 'USD', value: '5.00'},
    }],
  };
  pr.addEventListener('shippingaddresschange', function(e) {
    e.updateWith(updatedDetails);
  });
  pr.addEventListener('shippingoptionchange', function(e) {
    e.updateWith(updatedDetails);
  });
  showPaymentRequest(pr);
}

/**
 * Calls updateWith() with modifiers
 * @param {String} paymentMethod - the payment method to be used.
 */
function updateWithModifiers(paymentMethod) {
  const pr = buildPaymentRequest(paymentMethod);
  const updatedDetails = {
    modifiers: [{
      supportedMethods: paymentMethod,
      total: {
        label: 'Modifier total',
        amount: {currency: 'USD', value: '4.00'},
      },
      additionalDisplayItems: [{
        label: 'Discount',
        amount: {currency: 'USD', value: '-1.00'},
      }],
    }],
  };
  pr.addEventListener('shippingaddresschange', function(e) {
    e.updateWith(updatedDetails);
  });
  pr.addEventListener('shippingoptionchange', function(e) {
    e.updateWith(updatedDetails);
  });
  showPaymentRequest(pr);
}

/**
 * Calls updateWith() with an error.
 * @param {String} paymentMethod - the payment method to be used.
 */
function updateWithError(paymentMethod) {
  const pr = buildPaymentRequest(paymentMethod);
  const errorDetails = {
    error: 'This is an error for a browsertest',
  };
  pr.addEventListener('shippingaddresschange', function(e) {
    e.updateWith(errorDetails);
  });
  pr.addEventListener('shippingoptionchange', function(e) {
    e.updateWith(errorDetails);
  });
  showPaymentRequest(pr);
}
