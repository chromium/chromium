/*
 * Copyright 2016 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* global PaymentRequest:false */

var request;

/**
 * Launches the PaymentRequest UI that accepts credit cards.
 */
function ccBuy() { // eslint-disable-line no-unused-vars
  try {
    var details = {
      total: {
        label: 'Total',
        amount: {
          currency: 'USD',
          value: '5.00',
        },
      },
      shippingOptions: [{
        id: 'freeShippingOption',
        label: 'Free global shipping',
        amount: {
          currency: 'USD',
          value: '0',
        },
        selected: true,
      }],
    };
    request = new PaymentRequest(
        [{
          supportedMethods: 'basic-card',
          data: {supportedNetworks: ['visa']},
        }],
        {
          total: {
            label: 'Total',
            amount: {
              currency: 'USD',
              value: '5.00',
            },
          },
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
        {
          requestShipping: true,
        });
    request.show()
        .then(function(resp) {
          return resp.complete('success');
        })
        .then(function() {
          print(JSON.stringify(resp, undefined, 2));
        })
        .catch(function(error) {
          print(error);
        });
    request.addEventListener('shippingaddresschange', function(e) {
      e.updateWith(new Promise(function(resolve) {
        // No changes in price based on shipping address change.
        resolve(details);
      }));
    });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Launches the PaymentRequest UI which accepts only Android Pay.
 */
function androidPayBuy() { // eslint-disable-line no-unused-vars
  try {
    request = new PaymentRequest(
        [{
          supportedMethods: 'https://android.com/pay',
        }],
        {
          total: {
            label: 'Total',
            amount: {
              currency: 'USD',
              value: '5.00',
            },
          },
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
        {
          requestShipping: true,
        });
    request.show()
        .then(function(resp) {
          return resp.complete('success');
        })
        .then(function() {
          print(JSON.stringify(resp, undefined, 2));
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Launches the PaymentRequest UI which accepts only Android Pay and does not
 * require any other information.
 */
function androidPaySkipUiBuy() { // eslint-disable-line no-unused-vars
  try {
    request = new PaymentRequest(
        [{
          supportedMethods: 'https://android.com/pay',
        }],
        {
          total: {
            label: 'Total',
            amount: {
              currency: 'USD',
              value: '5.00',
            },
          },
        });
    request.show()
        .then(function(resp) {
          return resp.complete('success');
        })
        .then(function() {
          print(JSON.stringify(resp, undefined, 2));
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Launches the PaymentRequest UI which accepts only an unsupported payment
 * method.
 */
function noSupported() { // eslint-disable-line no-unused-vars
  try {
    request = new PaymentRequest(
        [{
          supportedMethods: 'https://randompay.com',
        }],
        {
          total: {
            label: 'Total',
            amount: {
              currency: 'USD',
              value: '5.00',
            },
          },
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
        {
          requestShipping: true,
        });
    request.show()
        .then(function(resp) {
          return resp.complete('success');
        })
        .then(function() {
          print(JSON.stringify(resp, undefined, 2));
        })
        .catch(function(error) {
          print(error);
        });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Launches the PaymentRequest UI which accepts credit cards and Bob Pay.
 */
function cardsAndBobPayBuy() { // eslint-disable-line no-unused-vars
  try {
    request = new PaymentRequest(
        [
          {supportedMethods: 'basic-card', data: {supportedNetworks: ['visa']}},
          {
            supportedMethods: 'https://bobpay.com',
          },
        ],
        {
          total: {
            label: 'Total',
            amount: {
              currency: 'USD',
              value: '5.00',
            },
          },
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
        {
          requestShipping: true,
        });
    request.show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(
                    resp.methodName + '<br>' +
                    JSON.stringify(resp.details, undefined, 2));
              })
              .catch(function(error) {
                print(error.message);
              });
        })
        .catch(function(error) {
          print(error.message);
        });
  } catch (error) {
    print(error.message);
  }
}

/**
 * Launches the PaymentRequest UI that requests contact information.
 */
function contactInfoBuy() { // eslint-disable-line no-unused-vars
  try {
    new PaymentRequest(
        [
          {supportedMethods: 'https://bobpay.com'},
          {
            supportedMethods: 'basic-card',
            data: {supportedNetworks: ['amex', 'visa']},
          },
        ],
        {total: {label: 'Total', amount: {currency: 'USD', value: '5.00'}}}, {
          requestPayerName: true,
          requestPayerEmail: true,
          requestPayerPhone: true,
        })
        .show()
        .then(function(resp) {
          resp.complete('success')
              .then(function() {
                print(
                    resp.payerName + '<br>' + resp.payerEmail + '<br>' +
                    resp.payerPhone + '<br>' + resp.methodName + '<br>' +
                    JSON.stringify(resp.details, undefined, 2));
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

/**
 * Aborts the current PaymentRequest.
 */
function abort() { // eslint-disable-line no-unused-vars
  try {
    request.abort()
        .then(function() {
          print('Aborted');
        })
        .catch(function() {
          print('Cannot abort');
        });
  } catch (error) {
    print(error.message);
  }
}
