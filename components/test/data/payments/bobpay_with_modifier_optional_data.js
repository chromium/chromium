/*
 * Copyright 2017 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * Launches the PaymentRequest UI with Bob Pay and Bob Pay modifier.
 */
function buy() {
  const methodData = [{supportedMethods: 'https://bobpay.test'}];
  try {
    new PaymentRequest(methodData, {
      total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
      modifiers: [{
        supportedMethods: 'https://bobpay.test',
        total: {
          label: 'Total',
          amount: {currency: 'USD', value: '4.00'},
        },
        additionalDisplayItems: [{
          label: 'BobPay discount',
          amount: {currency: 'USD', value: '-1.00'},
        }],
        data: {
          discountProgramParticipantId: '86328764873265',
        },
      }],
    })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(JSON.stringify(resp, undefined, 2));
              })
              .catch(function(error) {
                print('complete() rejected<br>' + error.message);
              });
        })
        .catch(function(error) {
          print('show() rejected<br>' + error.message);
        });
  } catch (error) {
    print('exception thrown<br>' + error.message);
  }
}

/**
 * Creates a payment request with required information and calls request.show()
 * to launch PaymentRequest UI.
 * @param {Object} options The list of requested paymentOptions.
 * @return {string} The 'success' or error message.
 */
function paymentRequestWithOptions(options) {
  try {
    const request = new PaymentRequest(
        [
          {supportedMethods: 'https://bobpay.xyz'},
        ],
        {
          total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}},
          shippingOptions: [{
            id: 'freeShippingOption',
            label: 'Free global shipping',
            amount: {
              currency: 'USD',
              value: '0',
            },
            selected: true,
          }],
        },
        options);
    request.show();
    return 'success';
  } catch (e) {
    return e.toString();
  }
}

/**
 * Launches the PaymentRequest UI with Bob Pay and shipping address requested.
 */
function buyWithShippingRequested() {
  paymentRequestWithOptions({
    requestShipping: true,
    requestPayerName: false,
    requestPayerEmail: false,
    requestPayerPhone: false,
  });
}

/**
 * Launches the PaymentRequest UI with Bob Pay and payer's contact details
 * requested.
 */
function buyWithContactRequested() {
  paymentRequestWithOptions({
    requestPayerName: true,
    requestPayerEmail: true,
    requestPayerPhone: true,
  });
}

/**
 * Launches the PaymentRequest UI with Bob Pay and both shipping address
 * and payer's contact details requested.
 */
function buyWithShippingAndContactRequested() {
  paymentRequestWithOptions({
    requestShipping: true,
    requestPayerName: true,
    requestPayerEmail: true,
    requestPayerPhone: true,
  });
}
