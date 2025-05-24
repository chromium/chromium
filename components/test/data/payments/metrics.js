/*
 * Copyright 2016 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

let request;

const bobPayMethod = Object.freeze({
  supportedMethods: 'https://bobpay.test',
});

const kylePayMethod = Object.freeze({
  supportedMethods: 'https://kylepay.test/webpay',
});

/**
 * Launches the PaymentRequest UI that accepts url payment methods.
 */
function buyWithUrlMethods() {
  buyWithMethods([bobPayMethod, kylePayMethod]);
}

/**
 * Launches the PaymentRequest UI that accepts credit cards.
 */
function ccBuy() {
  buyWithMethods([{
    supportedMethods: 'basic-card',
    data: {supportedNetworks: ['visa']},
  }]);
}

/**
 * Launches the PaymentRequest UI that accepts the given methods.
 * @param {Array<Object>} methods An array of payment method objects.
 */
 function buyWithMethods(methods) {
  try {
    const details = {
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
        methods,
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
          print(JSON.stringify(resp, undefined, 2));
          return resp.complete('success');
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
function androidPayBuy() {
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
function androidPaySkipUiBuy() {
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
 * @return {Promise<string>} - Either payment response as a JSON string or the
 * error message.
 */
async function noSupportedPromise() {
  try {
    const request = new PaymentRequest(
        [{
          supportedMethods: window.location.href + '/randompay',
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
    const response = await request.show();
    await response.complete('success');
    return JSON.stringify(response);
  } catch (error) {
    return error.toString();
  }
}

/**
 * Launches the PaymentRequest UI which accepts credit cards and Bob Pay.
 */
function cardsAndBobPayBuy() {
  try {
    request = new PaymentRequest(
        [
          {supportedMethods: 'basic-card', data: {supportedNetworks: ['visa']}},
          {
            supportedMethods: 'https://bobpay.test',
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
function contactInfoBuy() {
  try {
    new PaymentRequest(
        [
          {supportedMethods: 'https://bobpay.test'},
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
function abort() {
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
